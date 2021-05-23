/* 
 Orignal Blog Post: https://blog.svenpeter.dev/posts/m1_sprr_gxf/
 
 Comments, Build, Profiling Instructions added by dhoyt | @h02332 on May 23, 2021
 UBSAN Report with Xcode Profiling Instructions

BUILD 
 	clang -fprofile-instr-generate -fcoverage-mapping -mllvm -runtime-counter-relocation -g -fsanitize=undefined -O0 -o a.out cpde.c
 
PROFILE 
 	LLVM_PROFILE_FILE=default.profraw ./a.out
	xcrun llvm-profdata merge -sparse default.profraw -o da.profdata
	xcrun llvm-cov show ./a.out -instr-profile=dub.profdata

REPORT
 xcrun llvm-cov show ./a.out -instr-profile=dub.profdata --show-regions=0 -show-line-counts-or-regions -show-instantiation-summary
 xcrun llvm-cov report ./a.out -instr-profile=dub.profdata
 sudo gcovr --gcov-executable "xcrun llvm-cov gcov" -r . --html --html-details -o out.html

RUN
	LLVM_PROFILE_FILE=default.profraw ./a.out

COMMENT
	Errors Seen - Likely a Race Condition from pc +4 .. cx->uc_mcontext->__ss.__pc += 4; 
	Note that no crash is seen with libgmalloc, so the allocator needs more investigation
	
UNDEFINED BEHAVIOR SANITIZER OUTPUT - UBSAN
================================================

REPRO - SIP OFF, ALL M1 SECURITY OFF

LLVM_PROFILE_FILE=default.profraw ./a.out
...
code.c:25:9: runtime error: member access within misaligned address 0x00016d0471b8 for type 'ucontext_t' (aka 'struct __darwin_ucontext'), which requires 16 byte alignment
0x00016d0471b8: note: pointer points here
 00 00 00 00  00 00 00 00 00 00 00 00  50 71 04 6d 01 00 00 00  00 00 00 00 00 00 00 00  00 00 00 00
              ^ 
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior code.c:25:9 in 
code.c:25:9: runtime error: load of misaligned address 0x00016d0471e8 for type 'struct __darwin_mcontext64 *', which requires 16 byte alignment
0x00016d0471e8: note: pointer points here
 00 00 00 00  f0 71 04 6d 01 00 00 00  00 00 ea 02 01 00 00 00  0f 00 00 92 00 00 00 00  00 00 ea 02
              ^ 
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior code.c:25:9 in 
code.c:26:9: runtime error: member access within misaligned address 0x00016d0471b8 for type 'ucontext_t' (aka 'struct __darwin_ucontext'), which requires 16 byte alignment
0x00016d0471b8: note: pointer points here
 00 00 00 00  00 00 00 00 00 00 00 00  50 71 04 6d 01 00 00 00  00 00 00 00 00 00 00 00  00 00 00 00
              ^ 
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior code.c:26:9 in 
code.c:26:9: runtime error: load of misaligned address 0x00016d0471e8 for type 'struct __darwin_mcontext64 *', which requires 16 byte alignment
0x00016d0471e8: note: pointer points here
 00 00 00 00  f0 71 04 6d 01 00 00 00  00 00 ea 02 01 00 00 00  0f 00 00 92 00 00 00 00  41 41 41 41
              ^ 
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior code.c:26:9 in 

Thread 0 Crashed:: Dispatch queue: com.apple.main-thread
0   dyld                          	0x0000000102ea4000 0x102ea4000 + 0
1   code				                  	0x0000000102dbbcc4 sprr_test + 80 (code.c:92)
2   code        				          	0x0000000102dbb684 main + 172 (code.c:122)
3   libdyld.dylib                 	0x0000000186c89420 start + 4

Thread 0 crashed with ARM Thread State (64-bit):
    x0: 0x0000000041414141   x1: 0x0000000000000000   x2: 0x0000000000000007   x3: 0x0000000000041802
    x4: 0x00000000ffffffff   x5: 0x0000000000000000   x6: 0x0000000000000000   x7: 0x0000000000000000
    x8: 0x0000000102ea0000   x9: 0x0000000041414141  x10: 0x0000000000000000  x11: 0x00000001fe0525cb
   x12: 0x00000001fe0525cb  x13: 0x0000000000000033  x14: 0x0000000000000881  x15: 0x000000008000001f
   x16: 0x00000000000000c5  x17: 0x0000000102ec5f24  x18: 0x0000000000000000  x19: 0x2010000030000000
   x20: 0x0000000102ea0000  x21: 0x0000000000000000  x22: 0x0000000000000000  x23: 0x0000000000000000
   x24: 0x0000000000000000  x25: 0x0000000000000000  x26: 0x0000000000000000  x27: 0x0000000000000000
   x28: 0x000000016d047648   fp: 0x000000016d047590   lr: 0x0000000102dbbd98
    sp: 0x000000016d047590   pc: 0x0000000102ea4000 cpsr: 0x60000000
   far: 0x0000000102ea3ffc  esr: 0x02000000


Binary Images:
       0x102db8000 -        0x102dbbfff +smoke32-user (0) /Users/USER/*/smoke32-user
       0x102ea4000 -        0x102f23fff  dyld (851.27)  /usr/lib/dyld
       0x102fac000 -        0x102fd7fff +libclang_rt.ubsan_osx_dynamic.dylib (1205.0.22.9) ../clang/12.0.5/lib/darwin/libclang_rt.ubsan_osx_dynamic.dylib

*/

#define _XOPEN_SOURCE
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <ucontext.h>

/* Recover from Protected Page Access, setup Signal Handler, set x0 to a 0xdeadbeef (0x41414141) and increment program counter (pc), Return to ptr[0] = 0xd65f03c0. TODO, Fuzz the pc */
static void sev_handler(int signo, siginfo_t *info, void *cx_)
{
    (void)signo;
    (void)info;
    ucontext_t *cx = cx_;
    cx->uc_mcontext->__ss.__x[0] = 0xdeadbeef;
    cx->uc_mcontext->__ss.__pc = cx->uc_mcontext->__ss.__lr;
}

/* Recovering from Non-Executable Page Access, set x0 to 0xdeadbeef (0x41414141), set pc +4, jmp to link register (__ss.__lr) to Return */
static void bus_handler(int signo, siginfo_t *info, void *cx_)
{
    (void)signo;
    (void)info;
    ucontext_t *cx = cx_;
/* magic pattern like 0x41414141 or 0xdeadbeef*/
    cx->uc_mcontext->__ss.__x[0] = 0xdeadbeef;
/* Increase the Program Counter (pc) +4 on ARM - This is a jmp     
    cx->uc_mcontext->__ss.__pc += 4;
}

static void write_sprr_perm(uint64_t v)
{
/* Write the magic pattern  */
    __asm__ __volatile__("msr S3_6_c15_c1_5, %0\n"
                         "isb sy\n" ::"r"(v)
                         :);
}

static uint64_t read_sprr_perm(void)
{
    uint64_t v;
/* Read the magic pattern */
    __asm__ __volatile__("isb sy\n"
                         "mrs %0, S3_6_c15_c1_5\n"
                         : "=r"(v)::"memory");
    return v;
}

static bool can_read(void *ptr)
{
    uint64_t v = 0;
/* Check the Read on magic pattern and Return */
    __asm__ __volatile__("ldr x0, [%0]\n"
                         "mov %0, x0\n"
                         : "=r"(v)
                         : "r"(ptr)
                         : "memory", "x0");

    if (v == 0xdeadbeef)
        return false;
    return true;
}

static bool can_write(void *ptr)
{
    uint64_t v = 0;
/* Can we Write the magic pattern */
    __asm__ __volatile__("str x0, [%0]\n"
                         "mov %0, x0\n"
                         : "=r"(v)
                         : "r"(ptr + 8)
                         : "memory", "x0");

    if (v == 0xdeadbeef)
        return false;
    return true;
}

/* Can we Execute */
static bool can_exec(void *ptr)
{
    uint64_t (*fun_ptr)(uint64_t) = ptr;
    uint64_t res = fun_ptr(0);
    if (res == 0xdeadbeef)
        return false;
    return true;
}

static void sprr_test(void *ptr, uint64_t v)
{
    uint64_t a, b;
    a = read_sprr_perm();
    write_sprr_perm(v);
    b = read_sprr_perm();
/* Print Results */
    printf("%llx: %c%c%c\n", b, can_read(ptr) ? 'r' : '-', can_write(ptr) ? 'w' : '-',
           can_exec(ptr) ? 'x' : '-');
}

static uint64_t make_sprr_val(uint8_t nibble)
{
    uint64_t res = 0;
    for (int i = 0; i < 16; ++i)
/* Nibble 4 * i bytes at a time.. this is where undefined behavior creeps in ...  needs to be 16-byte aligned at all the time to not be undefined by C spec */
        res |= ((uint64_t)nibble) << (4 * i);
    return res;
}

/* MAIN FUNCTION */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    struct sigaction sa;
    sigfillset(&sa.sa_mask);
    sa.sa_sigaction = bus_handler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction(SIGBUS, &sa, 0);
    sa.sa_sigaction = sev_handler;
    sigaction(SIGSEGV, &sa, 0);

/* map a page with MAP_JIT and try to read, write or execute that memory for all four possible values in the system register */

    uint32_t *ptr = mmap(NULL, 0x4000, PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT, -1, 0);
   
		/* Flip some bits - 64-bit */
		   
    write_sprr_perm(0x3333333333333333);
    
    /* 
    Set memory to contain the RET instruction to attempt to execute. 
    There are multiple valid encodings of return (which is really a special form of branch). 
    These is the one clang seems to use: 
      kRet = 0xd65f03c0,
  		kBrk0 = 0xd4200000,
  		kBrk1 = 0xd4200020,
  		kBrkF000 = 0xd43e0000,
  		kHlt0 = 0xd4400000,
    */
    
    ptr[0] = 0xd65f03c0; // ret

    for (int i = 0; i < 4; ++i)
        sprr_test(ptr, make_sprr_val(i));
}

/* 

Interesting Results:

cx->uc_mcontext->__ss.__pc += 4;
RESULTS
2010000030000000: ---
2010000030100000: r-x
2010000030200000: r--
2010000030300000: rw-

vs

cx->uc_mcontext->__ss.__pc += 16;    
2010000030000000: rw-
2010000030100000: rwx
2010000030200000: rw-
2010000030300000: rw-


*/