// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandbox_init.h"

#if defined(OS_LINUX) && defined(__x86_64__)

#include <asm/unistd.h>
#include <errno.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <signal.h>
#include <string.h>
#include <sys/prctl.h>
#include <ucontext.h>
#include <unistd.h>

#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/time.h"
#include "content/public/common/content_switches.h"

#ifndef PR_SET_NO_NEW_PRIVS
  #define PR_SET_NO_NEW_PRIVS 38
#endif

#ifndef SYS_SECCOMP
  #define SYS_SECCOMP 1
#endif

#ifndef __NR_migrate_pages
  #define __NR_migrate_pages 256
#endif

#ifndef __NR_openat
  #define __NR_openat 257
#endif

#ifndef __NR_mkdirat
  #define __NR_mkdirat 258
#endif

#ifndef __NR_readlinkat
  #define __NR_readlinkat 267
#endif

#ifndef __NR_move_pages
  #define __NR_move_pages 279
#endif

#ifndef __NR_eventfd2
  #define __NR_eventfd2 290
#endif

#ifndef __NR_process_vm_readv
  #define __NR_process_vm_readv 310
#endif

#ifndef __NR_process_vm_writev
  #define __NR_process_vm_writev 311
#endif

// Constants from very new header files that we can't yet include.
#ifndef SECCOMP_MODE_FILTER
  #define SECCOMP_MODE_FILTER 2
  #define SECCOMP_RET_KILL        0x00000000U
  #define SECCOMP_RET_TRAP        0x00030000U
  #define SECCOMP_RET_ERRNO       0x00050000U
  #define SECCOMP_RET_ALLOW       0x7fff0000U
#endif


namespace {

static void CheckSingleThreaded(const std::string& process_type) {
  // Possibly racy, but it's ok because this is more of a debug check to catch
  // new threaded situations arising during development.
  int num_threads =
    file_util::CountFilesCreatedAfter(FilePath("/proc/self/task"),
                                      base::Time::UnixEpoch());

  // We pass the check if we don't know ( == 0), because the setuid sandbox
  // will prevent /proc access in some contexts.
  DCHECK((num_threads == 1 || num_threads == 0)) << "Counted "
                                                 << num_threads << " threads "
                                                 << "in " << process_type
                                                 << ".";
}

static void SIGSYS_Handler(int signal, siginfo_t* info, void* void_context) {
  if (signal != SIGSYS)
    return;
  if (info->si_code != SYS_SECCOMP)
    return;
  if (!void_context)
    return;
  ucontext_t* context = reinterpret_cast<ucontext_t*>(void_context);
  uintptr_t syscall = context->uc_mcontext.gregs[REG_RAX];
  if (syscall >= 1024)
    syscall = 0;
  // Encode 8-bits of the 1st two arguments too, so we can discern which socket
  // type, which fcntl, ... etc., without being likely to hit a mapped
  // address.
  // Do not encode more bits here without thinking about increasing the
  // likelihood of collision with mapped pages.
  syscall |= ((context->uc_mcontext.gregs[REG_RDI] & 0xffUL) << 12);
  syscall |= ((context->uc_mcontext.gregs[REG_RSI] & 0xffUL) << 20);
  // Purposefully dereference the syscall as an address so it'll show up very
  // clearly and easily in crash dumps.
  volatile char* addr = reinterpret_cast<volatile char*>(syscall);
  *addr = '\0';
  // In case we hit a mapped address, hit the null page with just the syscall,
  // for paranoia.
  syscall &= 0xfffUL;
  addr = reinterpret_cast<volatile char*>(syscall);
  *addr = '\0';
  _exit(1);
}

static void InstallSIGSYSHandler() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = SIGSYS_Handler;
  int ret = sigaction(SIGSYS, &sa, NULL);
  PLOG_IF(FATAL, ret != 0) << "Failed to install SIGSYS handler.";
}

static void EmitLoad(int offset, std::vector<struct sock_filter>* program) {
  struct sock_filter filter;
  filter.code = BPF_LD+BPF_W+BPF_ABS;
  filter.jt = 0;
  filter.jf = 0;
  filter.k = offset;
  program->push_back(filter);
}

static void EmitLoadArg(int arg, std::vector<struct sock_filter>* program) {
  // Each argument is 8 bytes, independent of architecture, and start at
  // an offset of 16 bytes, indepdendent of architecture.
  EmitLoad(((arg - 1) * 8) + 16, program);
}

static void EmitJEQJT1(int value, std::vector<struct sock_filter>* program) {
  struct sock_filter filter;
  filter.code = BPF_JMP+BPF_JEQ+BPF_K;
  filter.jt = 1;
  filter.jf = 0;
  filter.k = value;
  program->push_back(filter);
}

static void EmitJEQJF(int value,
                      int jf,
                      std::vector<struct sock_filter>* program) {
  struct sock_filter filter;
  filter.code = BPF_JMP+BPF_JEQ+BPF_K;
  filter.jt = 0;
  filter.jf = jf;
  filter.k = value;
  program->push_back(filter);
}

static void EmitRet(int value, std::vector<struct sock_filter>* program) {
  struct sock_filter filter;
  filter.code = BPF_RET+BPF_K;
  filter.jt = 0;
  filter.jf = 0;
  filter.k = value;
  program->push_back(filter);
}

static void EmitPreamble(std::vector<struct sock_filter>* program) {
  // First, check correct syscall arch.
  // "4" is magic offset for the arch number.
  EmitLoad(4, program);
  EmitJEQJT1(AUDIT_ARCH_X86_64, program);
  EmitRet(SECCOMP_RET_KILL, program);

  // Load the syscall number.
  // "0" is magic offset for the syscall number.
  EmitLoad(0, program);
}

static void EmitTrap(std::vector<struct sock_filter>* program) {
  EmitRet(SECCOMP_RET_TRAP, program);
}

static void EmitAllow(std::vector<struct sock_filter>* program) {
  EmitRet(SECCOMP_RET_ALLOW, program);
}

static void EmitAllowSyscall(int nr, std::vector<struct sock_filter>* program) {
  EmitJEQJF(nr, 1, program);
  EmitAllow(program);
}

static void EmitDenySyscall(int nr, std::vector<struct sock_filter>* program) {
  EmitJEQJF(nr, 1, program);
  EmitTrap(program);
}

static void EmitAllowSyscallArgN(int nr,
                                 int arg_nr,
                                 int arg_val,
                                 std::vector<struct sock_filter>* program) {
  // Jump forward 4 on no-match so that we also skip the unneccessary reload of
  // syscall_nr. (It is unneccessary because we have not trashed it yet.)
  EmitJEQJF(nr, 4, program);
  EmitLoadArg(arg_nr, program);
  EmitJEQJF(arg_val, 1, program);
  EmitAllow(program);
  // We trashed syscall_nr so put it back in the accumulator.
  EmitLoad(0, program);
}

static void EmitFailSyscall(int nr, int err,
                            std::vector<struct sock_filter>* program) {
  EmitJEQJF(nr, 1, program);
  EmitRet(SECCOMP_RET_ERRNO | err, program);
}

// TODO(cevans) -- only really works as advertised once we restrict clone()
// to CLONE_THREAD.
static void EmitAllowSignalSelf(std::vector<struct sock_filter>* program) {
  EmitAllowSyscallArgN(__NR_kill, 1, getpid(), program);
  EmitAllowSyscallArgN(__NR_tgkill, 1, getpid(), program);
}

static void EmitAllowGettime(std::vector<struct sock_filter>* program) {
  EmitAllowSyscall(__NR_clock_gettime, program);
  EmitAllowSyscall(__NR_gettimeofday, program);
  EmitAllowSyscall(__NR_time, program);
}

static void EmitSetupEmptyFileSystem(std::vector<struct sock_filter>* program) {
  EmitFailSyscall(__NR_open, ENOENT, program);
  EmitFailSyscall(__NR_openat, ENOENT, program);
  EmitFailSyscall(__NR_execve, ENOENT, program);
  EmitFailSyscall(__NR_access, ENOENT, program);
  EmitFailSyscall(__NR_mkdir, ENOENT, program);
  EmitFailSyscall(__NR_mkdirat, ENOENT, program);
  EmitFailSyscall(__NR_readlink, ENOENT, program);
  EmitFailSyscall(__NR_readlinkat, ENOENT, program);
  EmitFailSyscall(__NR_stat, ENOENT, program);
  EmitFailSyscall(__NR_lstat, ENOENT, program);
  EmitFailSyscall(__NR_chdir, ENOENT, program);
}

static void ApplyGPUPolicy(std::vector<struct sock_filter>* program) {
  // "Hot" syscalls go first.
  EmitAllowSyscall(__NR_read, program);
  EmitAllowSyscall(__NR_ioctl, program);
  EmitAllowSyscall(__NR_poll, program);
  EmitAllowSyscall(__NR_epoll_wait, program);
  EmitAllowSyscall(__NR_recvfrom, program);
  EmitAllowSyscall(__NR_write, program);
  EmitAllowSyscall(__NR_writev, program);
  EmitAllowSyscall(__NR_gettid, program);
  EmitAllowSyscall(__NR_sched_yield, program);  // Nvidia binary driver.
  EmitAllowGettime(program);

  // Less hot syscalls.
  EmitAllowSyscall(__NR_futex, program);
  EmitAllowSyscall(__NR_madvise, program);
  EmitAllowSyscall(__NR_sendmsg, program);
  EmitAllowSyscall(__NR_recvmsg, program);
  EmitAllowSyscall(__NR_eventfd2, program);
  EmitAllowSyscall(__NR_pipe, program);
  EmitAllowSyscall(__NR_mmap, program);
  EmitAllowSyscall(__NR_mprotect, program);
  // TODO(cevans): restrict flags.
  EmitAllowSyscall(__NR_clone, program);
  EmitAllowSyscall(__NR_set_robust_list, program);
  EmitAllowSyscall(__NR_getuid, program);
  EmitAllowSyscall(__NR_geteuid, program);
  EmitAllowSyscall(__NR_getgid, program);
  EmitAllowSyscall(__NR_getegid, program);
  EmitAllowSyscall(__NR_epoll_create, program);
  EmitAllowSyscall(__NR_fcntl, program);
  EmitAllowSyscall(__NR_socketpair, program);
  EmitAllowSyscall(__NR_epoll_ctl, program);
  EmitAllowSyscall(__NR_prctl, program);
  EmitAllowSyscall(__NR_fstat, program);
  EmitAllowSyscall(__NR_close, program);
  EmitAllowSyscall(__NR_restart_syscall, program);
  EmitAllowSyscall(__NR_rt_sigreturn, program);
  EmitAllowSyscall(__NR_brk, program);
  EmitAllowSyscall(__NR_rt_sigprocmask, program);
  EmitAllowSyscall(__NR_munmap, program);
  EmitAllowSyscall(__NR_dup, program);
  EmitAllowSyscall(__NR_mlock, program);
  EmitAllowSyscall(__NR_munlock, program);
  EmitAllowSyscall(__NR_exit, program);
  EmitAllowSyscall(__NR_exit_group, program);
  EmitAllowSyscall(__NR_getpid, program);  // Nvidia binary driver.
  EmitAllowSyscall(__NR_getppid, program);  // ATI binary driver.
  EmitAllowSyscall(__NR_lseek, program);  // Nvidia binary driver.
  EmitAllowSyscall(__NR_shutdown, program);  // Virtual driver.
  EmitAllowSyscall(__NR_rt_sigaction, program);  // Breakpad signal handler.
  EmitFailSyscall(__NR_socket, EACCES, program);  // Nvidia binary driver.
  EmitAllowSignalSelf(program);  // GPU watchdog.

  // Generally, filename-based syscalls will fail with ENOENT to behave
  // similarly to a possible future setuid sandbox.
  EmitSetupEmptyFileSystem(program);
}

static void ApplyFlashPolicy(std::vector<struct sock_filter>* program) {
  // "Hot" syscalls go first.
  EmitAllowSyscall(__NR_futex, program);
  EmitAllowSyscall(__NR_write, program);
  EmitAllowSyscall(__NR_epoll_wait, program);
  EmitAllowSyscall(__NR_read, program);
  EmitAllowSyscall(__NR_times, program);

  // Less hot syscalls.
  EmitAllowGettime(program);
  // TODO(cevans): restrict flags.
  EmitAllowSyscall(__NR_clone, program);
  EmitAllowSyscall(__NR_set_robust_list, program);
  EmitAllowSyscall(__NR_getuid, program);
  EmitAllowSyscall(__NR_geteuid, program);
  EmitAllowSyscall(__NR_getgid, program);
  EmitAllowSyscall(__NR_getegid, program);
  EmitAllowSyscall(__NR_epoll_create, program);
  EmitAllowSyscall(__NR_fcntl, program);
  EmitAllowSyscall(__NR_socketpair, program);
  EmitAllowSyscall(__NR_pipe, program);
  EmitAllowSyscall(__NR_epoll_ctl, program);
  EmitAllowSyscall(__NR_gettid, program);
  EmitAllowSyscall(__NR_prctl, program);
  EmitAllowSyscall(__NR_fstat, program);
  EmitAllowSyscall(__NR_sendmsg, program);
  EmitAllowSyscall(__NR_mmap, program);
  EmitAllowSyscall(__NR_munmap, program);
  EmitAllowSyscall(__NR_mprotect, program);
  EmitAllowSyscall(__NR_madvise, program);
  EmitAllowSyscall(__NR_rt_sigaction, program);
  EmitAllowSyscall(__NR_rt_sigprocmask, program);
  EmitAllowSyscall(__NR_wait4, program);
  EmitAllowSyscall(__NR_exit_group, program);
  EmitAllowSyscall(__NR_exit, program);
  EmitAllowSyscall(__NR_rt_sigreturn, program);
  EmitAllowSyscall(__NR_restart_syscall, program);
  EmitAllowSyscall(__NR_close, program);
  EmitAllowSyscall(__NR_recvmsg, program);
  EmitAllowSyscall(__NR_lseek, program);
  EmitAllowSyscall(__NR_brk, program);
  EmitAllowSyscall(__NR_sched_yield, program);
  EmitAllowSyscall(__NR_shutdown, program);
  EmitAllowSyscall(__NR_sched_getaffinity, program);  // 3D
  EmitAllowSyscall(__NR_dup, program);  // Flash Access.
  // TODO(jorgelo): restrict this.
  EmitAllowSyscall(__NR_ioctl, program);  // Flash Access.
  EmitAllowSignalSelf(program);

  // These are under investigation, and hopefully not here for the long term.
  EmitAllowSyscall(__NR_shmctl, program);
  EmitAllowSyscall(__NR_shmat, program);
  EmitAllowSyscall(__NR_shmdt, program);

  EmitSetupEmptyFileSystem(program);
}

static void ApplyNoPtracePolicy(std::vector<struct sock_filter>* program) {
  EmitDenySyscall(__NR_ptrace, program);
  EmitDenySyscall(__NR_process_vm_readv, program);
  EmitDenySyscall(__NR_process_vm_writev, program);
  EmitDenySyscall(__NR_migrate_pages, program);
  EmitDenySyscall(__NR_move_pages, program);
}

static bool CanUseSeccompFilters() {
  int ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, 0, 0, 0);
  if (ret != 0 && errno == EFAULT)
    return true;
  return false;
}

static void InstallFilter(const std::vector<struct sock_filter>& program) {
  int ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
  PLOG_IF(FATAL, ret != 0) << "prctl(PR_SET_NO_NEW_PRIVS) failed";

  struct sock_fprog fprog;
  fprog.len = program.size();
  fprog.filter = const_cast<struct sock_filter*>(&program[0]);

  ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &fprog, 0, 0);
  PLOG_IF(FATAL, ret != 0) << "Failed to install filter.";
}

static bool ShouldEnableGPUSandbox() {
  // Default setting is: enabled for Linux, disabled for Chrome OS.
  // '--disable-gpu-sandbox' takes precedence over '--enable-gpu-sandbox'.
#if defined(OS_CHROMEOS)
  bool res = false;
#else
  bool res = true;
#endif

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kEnableGpuSandbox)) {
    res = true;
  }
  if (command_line.HasSwitch(switches::kDisableGpuSandbox)) {
    res = false;
  }

  return res;
}

}  // anonymous namespace

namespace content {

void InitializeSandbox() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kNoSandbox) ||
      command_line.HasSwitch(switches::kDisableSeccompFilterSandbox))
    return;

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type == switches::kGpuProcess &&
      !ShouldEnableGPUSandbox())
    return;

  if (!CanUseSeccompFilters())
    return;

  CheckSingleThreaded(process_type);

  std::vector<struct sock_filter> program;
  EmitPreamble(&program);

  if (process_type == switches::kGpuProcess) {
    ApplyGPUPolicy(&program);
    EmitTrap(&program);  // Default deny.
  } else if (process_type == switches::kPpapiPluginProcess) {
    ApplyFlashPolicy(&program);
    EmitTrap(&program);  // Default deny.
  } else if (process_type == switches::kRendererProcess ||
             process_type == switches::kWorkerProcess) {
    ApplyNoPtracePolicy(&program);
    EmitAllow(&program);  // Default permit.
  } else {
    NOTREACHED();
  }

  InstallSIGSYSHandler();
  InstallFilter(program);
}

}  // namespace content

#else

namespace content {

void InitializeSandbox() {
}

}  // namespace content

#endif
