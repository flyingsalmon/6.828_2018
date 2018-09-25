// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}


int mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
    uint32_t *ebp;
    struct Eipdebuginfo info;
    int result;

    ebp = (uint32_t *)read_ebp();

    cprintf("Stack backtrace:\r\n");

    while (ebp)
    {
        cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\r\n", ebp, ebp[1], ebp[2], ebp[3], ebp[4], ebp[5], ebp[6]);

        memset(&info, 0, sizeof(struct Eipdebuginfo));

        result = debuginfo_eip(ebp[1], &info);
        if (0 != result)
        {
            cprintf("failed to get debuginfo for eip %x.\r\n", ebp[1]);
        }
        else
        {
            cprintf("\t%s:%d: %.*s+%u\r\n", info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, ebp[1] - info.eip_fn_addr);
        }

        ebp = (uint32_t *)*ebp;
    }

	return 0;
}

// int
// mon_backtrace(int argc, char **argv, struct Trapframe *tf)
// {
// 	// Your code here.
// 	extern char bootstacktop[], bootstack[];

// 	struct Eipdebuginfo info;

// 	//cprintf("bootstack %x bootstacktop %x\n", bootstack, bootstacktop);

// 	unsigned int ebp;

// 	ebp = read_ebp();

// 	while (ebp) {
// 		cprintf("ebp %x eip %x args %x %x %x %x %x\n", ebp, 
// 												*(unsigned int *)(ebp + 4), 	// return addr
// 												*(unsigned int *)(ebp + 8),		// arg1
// 												*(unsigned int *)(ebp + 12),	// arg2
// 												*(unsigned int *)(ebp + 16),	// arg3
// 												*(unsigned int *)(ebp + 20), 	// arg4
// 												*(unsigned int *)(ebp + 24));	// arg5
		
// 		debuginfo_eip(*(unsigned int *)(ebp + 4), &info);
		
// 		ebp = *(unsigned int *)ebp;
// 	}

// 	return 0;
// }



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
