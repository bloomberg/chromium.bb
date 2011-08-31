/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This is a standalone program that loads and runs the dynamic linker.
 * This program itself must be linked statically.  To keep it small, it's
 * written to avoid all dependencies on libc and standard startup code.
 * Hence, this should be linked using -nostartfiles.  It must be compiled
 * with -fno-stack-protector to ensure the compiler won't emit code that
 * presumes some special setup has been done.
 *
 * On ARM, the compiler will emit calls to some libc functions, so we
 * cannot link with -nostdlib.  The functions it does use (memset and
 * __aeabi_* functions for integer division) are sufficiently small and
 * self-contained in ARM's libc.a that we don't have any problem using
 * the libc definitions though we aren't using the rest of libc or doing
 * any of the setup it might expect.
 */

#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>

#define MAX_PHNUM               12

#if defined(__i386__)
# define DYNAMIC_LINKER "/lib/ld-linux.so.2"
#elif defined(__x86_64__)
# define DYNAMIC_LINKER "/lib64/ld-linux-x86-64.so.2"
#elif defined(__ARM_EABI__)
# define DYNAMIC_LINKER "/lib/ld-linux.so.3"
#else
# error "Don't know the dynamic linker file name for this architecture!"
#endif


/*
 * We're not using <string.h> functions here, to avoid dependencies.
 * In the x86 libc, even "simple" functions like memset and strlen can
 * depend on complex startup code, because in newer libc
 * implementations they are defined using STT_GNU_IFUNC.
 */

static void my_bzero(void *buf, size_t n) {
  char *p = buf;
  while (n-- > 0)
    *p++ = 0;
}

static size_t my_strlen(const char *s) {
  size_t n = 0;
  while (*s++ != '\0')
    ++n;
  return n;
}


/*
 * Get inline functions for system calls.
 */
static int my_errno;
#define SYS_ERRNO my_errno
#include "third_party/lss/linux_syscall_support.h"


/*
 * We're avoiding libc, so no printf.  The only nontrivial thing we need
 * is rendering numbers, which is, in fact, pretty trivial.
 */
static void iov_int_string(int value, struct kernel_iovec *iov,
                           char *buf, size_t bufsz) {
  char *p = &buf[bufsz];
  do {
    --p;
    *p = "0123456789"[value % 10];
    value /= 10;
  } while (value != 0);
  iov->iov_base = p;
  iov->iov_len = &buf[bufsz] - p;
}

#define STRING_IOV(string_constant, cond) \
  { (void *) string_constant, cond ? (sizeof(string_constant) - 1) : 0 }

__attribute__((noreturn)) static void fail(const char *message,
                                           const char *item1, int value1,
                                           const char *item2, int value2) {
  char valbuf1[32];
  char valbuf2[32];
  struct kernel_iovec iov[] = {
    STRING_IOV("bootstrap_helper", 1),
    STRING_IOV(DYNAMIC_LINKER, 1),
    STRING_IOV(": ", 1),
    { (void *) message, my_strlen(message) },
    { (void *) item1, item1 == NULL ? 0 : my_strlen(item1) },
    STRING_IOV("=", item1 != NULL),
    {},
    STRING_IOV(", ", item1 != NULL && item2 != NULL),
    { (void *) item2, item2 == NULL ? 0 : my_strlen(item2) },
    STRING_IOV("=", item2 != NULL),
    {},
    { "\n", 1 },
  };
  const int niov = sizeof(iov) / sizeof(iov[0]);

  if (item1 != NULL)
    iov_int_string(value1, &iov[6], valbuf1, sizeof(valbuf1));
  if (item2 != NULL)
    iov_int_string(value1, &iov[10], valbuf2, sizeof(valbuf2));

  sys_writev(2, iov, niov);
  sys_exit_group(2);
  while (1) *(volatile int *) 0 = 0;  /* Crash.  */
}


static int my_open(const char *file, int oflag) {
  int result = sys_open(file, oflag, 0);
  if (result < 0)
    fail("Cannot open dynamic linker!  ", "errno", my_errno, NULL, 0);
  return result;
}

static void my_pread(const char *fail_message,
                     int fd, void *buf, size_t bufsz, uintptr_t pos) {
  ssize_t result = sys_pread64(fd, buf, bufsz, pos);
  if (result < 0)
    fail(fail_message, "errno", my_errno, NULL, 0);
  if ((size_t) result != bufsz)
    fail(fail_message, "read count", result, NULL, 0);
}

static uintptr_t my_mmap(const char *segment_type, unsigned int segnum,
                         uintptr_t address, size_t size,
                         int prot, int flags, int fd, uintptr_t pos) {
#if defined(__NR_mmap2)
  void *result = sys_mmap2((void *) address, size, prot, flags, fd, pos >> 12);
#else
  void *result = sys_mmap((void *) address, size, prot, flags, fd, pos);
#endif
  if (result == MAP_FAILED)
    fail("Failed to map from dynamic linker!  ",
         segment_type, segnum, "errno", my_errno);
  return (uintptr_t) result;
}

static void my_mprotect(unsigned int segnum,
                        uintptr_t address, size_t size, int prot) {
  if (sys_mprotect((void *) address, size, prot) < 0)
    fail("Failed to mprotect hole in dynamic linker!  ",
         "segment", segnum, "errno", my_errno);
}


static int prot_from_phdr(const ElfW(Phdr) *phdr) {
  int prot = 0;
  if (phdr->p_flags & PF_R)
    prot |= PROT_READ;
  if (phdr->p_flags & PF_W)
    prot |= PROT_WRITE;
  if (phdr->p_flags & PF_X)
    prot |= PROT_EXEC;
  return prot;
}

static uintptr_t round_up(uintptr_t value, uintptr_t size) {
  return (value + size - 1) & -size;
}

static uintptr_t round_down(uintptr_t value, uintptr_t size) {
  return value & -size;
}

/*
 * Handle the "bss" portion of a segment, where the memory size
 * exceeds the file size and we zero-fill the difference.  For any
 * whole pages in this region, we over-map anonymous pages.  For the
 * sub-page remainder, we zero-fill bytes directly.
 */
static void handle_bss(unsigned int segnum, const ElfW(Phdr) *ph,
                       ElfW(Addr) load_bias, size_t pagesize) {
  if (ph->p_memsz > ph->p_filesz) {
    ElfW(Addr) file_end = ph->p_vaddr + load_bias + ph->p_filesz;
    ElfW(Addr) file_page_end = round_up(file_end, pagesize);
    ElfW(Addr) page_end = round_up(ph->p_vaddr + load_bias +
                                   ph->p_memsz, pagesize);
    if (page_end > file_page_end)
      my_mmap("bss segment", segnum,
              file_page_end, page_end - file_page_end,
              prot_from_phdr(ph), MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
    if (file_page_end > file_end && (ph->p_flags & PF_W))
      my_bzero((void *) file_end, file_page_end - file_end);
  }
}

/*
 * This is the main loading code.  It's called with the address of the
 * auxiliary vector on the stack, which we need to examine and modify.
 * It returns the dynamic linker's runtime entry point address, where
 * we should jump to.  This is called by the machine-dependent _start
 * code (below).  On return, it restores the original stack pointer
 * and jumps to this entry point.
 */
ElfW(Addr) do_load(ElfW(auxv_t) *auxv) {
  /*
   * Record the auxv entries that are specific to the file loaded.
   * The incoming entries point to our own static executable.
   */
  ElfW(auxv_t) *av_entry = NULL;
  ElfW(auxv_t) *av_phdr = NULL;
  ElfW(auxv_t) *av_phnum = NULL;
  size_t pagesize = 0;

  ElfW(auxv_t) *av;
  for (av = auxv;
       av_entry == NULL || av_phdr == NULL || av_phnum == NULL || pagesize == 0;
       ++av) {
    switch (av->a_type) {
      case AT_NULL:
        fail("Failed to find AT_ENTRY, AT_PHDR, AT_PHNUM, or AT_PAGESZ!",
             NULL, 0, NULL, 0);
        /*NOTREACHED*/
        break;
      case AT_ENTRY:
        av_entry = av;
        break;
      case AT_PAGESZ:
        pagesize = av->a_un.a_val;
        break;
      case AT_PHDR:
        av_phdr = av;
        break;
      case AT_PHNUM:
        av_phnum = av;
        break;
    }
  }

  int fd = my_open(DYNAMIC_LINKER, O_RDONLY);

  ElfW(Ehdr) ehdr;
  my_pread("Failed to read ELF header from dynamic linker!  ",
           fd, &ehdr, sizeof(ehdr), 0);

  if (ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
      ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
      ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
      ehdr.e_ident[EI_MAG3] != ELFMAG3 ||
      ehdr.e_version != EV_CURRENT ||
      ehdr.e_ehsize != sizeof(ehdr) ||
      ehdr.e_phentsize != sizeof(ElfW(Phdr)))
    fail("Dynamic linker has no valid ELF header!", NULL, 0, NULL, 0);

  switch (ehdr.e_machine) {
#if defined(__i386__)
    case EM_386:
#elif defined(__x86_64__)
    case EM_X86_64:
#elif defined(__arm__)
    case EM_ARM:
#else
# error "Don't know the e_machine value for this architecture!"
#endif
      break;
    default:
      fail("Dynamic linker has wrong architecture!  ",
           "e_machine", ehdr.e_machine, NULL, 0);
      break;
  }

  ElfW(Phdr) phdr[MAX_PHNUM];
  if (ehdr.e_phnum > sizeof(phdr) / sizeof(phdr[0]) || ehdr.e_phnum < 1)
    fail("Dynamic linker has unreasonable ",
         "e_phnum", ehdr.e_phnum, NULL, 0);

  if (ehdr.e_type != ET_DYN)
    fail("Dynamic linker not ET_DYN!  ",
         "e_type", ehdr.e_type, NULL, 0);

  my_pread("Failed to read program headers from dynamic linker!  ",
           fd, phdr, sizeof(phdr[0]) * ehdr.e_phnum, ehdr.e_phoff);

  size_t i = 0;
  while (i < ehdr.e_phnum && phdr[i].p_type != PT_LOAD)
    ++i;
  if (i == ehdr.e_phnum)
    fail("Dynamic linker has no PT_LOAD header!",
         NULL, 0, NULL, 0);

  /*
   * ELF requires that PT_LOAD segments be in ascending order of p_vaddr.
   * Find the last one to calculate the whole address span of the image.
   */
  const ElfW(Phdr) *first_load = &phdr[i];
  const ElfW(Phdr) *last_load = &phdr[ehdr.e_phnum - 1];
  while (last_load > first_load && last_load->p_type != PT_LOAD)
    --last_load;

  size_t span = last_load->p_vaddr + last_load->p_memsz - first_load->p_vaddr;

  /*
   * Map the first segment and reserve the space used for the rest and
   * for holes between segments.
   */
  const uintptr_t mapping = my_mmap("segment", first_load - phdr,
                                    round_down(first_load->p_vaddr, pagesize),
                                    span, prot_from_phdr(first_load),
                                    MAP_PRIVATE, fd,
                                    round_down(first_load->p_offset, pagesize));

  const ElfW(Addr) load_bias = mapping - round_down(first_load->p_vaddr,
                                                    pagesize);

  if (first_load->p_offset > ehdr.e_phoff ||
      first_load->p_filesz < ehdr.e_phoff + (ehdr.e_phnum * sizeof(ElfW(Phdr))))
    fail("First load segment of dynamic linker does not contain phdrs!",
         NULL, 0, NULL, 0);

  /* Point the auxv elements at the dynamic linker's phdrs and entry.  */
  av_phdr->a_un.a_val = (ehdr.e_phoff - first_load->p_offset +
                         first_load->p_vaddr + load_bias);
  av_phnum->a_un.a_val = ehdr.e_phnum;
  av_entry->a_un.a_val = ehdr.e_entry + load_bias;

  handle_bss(first_load - phdr, first_load, load_bias, pagesize);

  ElfW(Addr) last_end = first_load->p_vaddr + load_bias + first_load->p_memsz;

  /*
   * Map the remaining segments, and protect any holes between them.
   */
  const ElfW(Phdr) *ph;
  for (ph = first_load + 1; ph <= last_load; ++ph) {
    if (ph->p_type == PT_LOAD) {
      ElfW(Addr) last_page_end = round_up(last_end, pagesize);

      last_end = ph->p_vaddr + load_bias + ph->p_memsz;
      ElfW(Addr) start = round_down(ph->p_vaddr + load_bias, pagesize);
      ElfW(Addr) end = round_up(last_end, pagesize);

      if (start > last_page_end)
        my_mprotect(ph - phdr, last_page_end, start - last_page_end, PROT_NONE);

      my_mmap("segment", ph - phdr,
              start, end - start,
              prot_from_phdr(ph), MAP_PRIVATE | MAP_FIXED, fd,
              round_down(ph->p_offset, pagesize));

      handle_bss(ph - phdr, ph, load_bias, pagesize);
    }
  }

  sys_close(fd);

  return ehdr.e_entry + load_bias;
}

/*
 * We have to define the actual entry point code (_start) in assembly
 * for each machine.  The kernel startup protocol is not compatible
 * with the normal C function calling convention.  Here, we calculate
 * the address of the auxiliary vector on the stack; call do_load
 * (above) using the normal C convention as per the ABI; restore the
 * original starting stack; and finally, jump to the dynamic linker's
 * entry point address.
 */
#if defined(__i386__)
asm(".globl _start\n"
    ".type _start,@function\n"
    "_start:\n"
    "xorl %ebp, %ebp\n"
    "movl %esp, %ebx\n"         /* Save starting SP in %ebx.  */
    "andl $-16, %esp\n"         /* Align the stack as per ABI.  */
    "movl (%ebx), %eax\n"       /* argc */
    "leal 8(%ebx,%eax,4), %ecx\n" /* envp */
    /* Find the envp element that is NULL, and auxv is past there.  */
    "0: addl $4, %ecx\n"
    "cmpl $0, -4(%ecx)\n"
    "jne 0b\n"
    "pushl %ecx\n"              /* Argument: auxv.  */
    "call do_load\n"
    "movl %ebx, %esp\n"         /* Restore the saved SP.  */
    "jmp *%eax\n"               /* Jump to the entry point.  */
    );
#elif defined(__x86_64__)
asm(".globl _start\n"
    ".type _start,@function\n"
    "_start:\n"
    "xorq %rbp, %rbp\n"
    "movq %rsp, %rbx\n"         /* Save starting SP in %rbx.  */
    "andq $-16, %rsp\n"         /* Align the stack as per ABI.  */
    "movq (%rbx), %rax\n"       /* argc */
    "leaq 16(%rbx,%rax,8), %rdi\n"  /* envp */
    /* Find the envp element that is NULL, and auxv is past there.  */
    "0: addq $8, %rdi\n"
    "cmpq $0, -8(%rdi)\n"
    "jne 0b\n"
    "call do_load\n"            /* Argument already in %rdi: auxv */
    "movq %rbx, %rsp\n"         /* Restore the saved SP.  */
    "jmp *%rax\n"               /* Jump to the entry point.  */
    );
#elif defined(__arm__)
asm(".globl _start\n"
    ".type _start,#function\n"
    "_start:\n"
#if defined(__thumb2__)
    ".thumb\n"
    ".syntax unified\n"
#endif
    "mov fp, #0\n"
    "mov lr, #0\n"
    "mov r4, sp\n"              /* Save starting SP in r4.  */
    "ldr r1, [r4]\n"            /* argc */
    "add r1, r1, #2\n"
    "add r0, r4, r1, asl #2\n"  /* envp */
    /* Find the envp element that is NULL, and auxv is past there.  */
    "0: ldr r1, [r0], #4\n"
    "cmp r1, #0\n"
    "bne 0b\n"
    "bl do_load\n"
    "mov sp, r4\n"              /* Restore the saved SP.  */
    "blx r0\n"                  /* Jump to the entry point.  */
    );
#else
# error "Need stack-preserving _start code for this architecture!"
#endif
