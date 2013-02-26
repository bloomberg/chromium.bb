// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <asm/unistd.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ucontext.h>
#include <unistd.h>

#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "content/common/sandbox_linux.h"
#include "content/common/sandbox_seccomp_bpf_linux.h"
#include "content/public/common/content_switches.h"
#include "sandbox/linux/services/broker_process.h"

// These are the only architectures supported for now.
#if defined(__i386__) || defined(__x86_64__) || \
    (defined(__arm__) && (defined(__thumb__) || defined(__ARM_EABI__)))
#define SECCOMP_BPF_SANDBOX
#endif

#if defined(SECCOMP_BPF_SANDBOX)
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/linux_syscalls.h"

using playground2::arch_seccomp_data;
using playground2::ErrorCode;
using playground2::Sandbox;
using sandbox::BrokerProcess;

namespace {

void StartSandboxWithPolicy(Sandbox::EvaluateSyscall syscall_policy,
                            BrokerProcess* broker_process);

inline bool RunningOnASAN() {
#if defined(ADDRESS_SANITIZER)
  return true;
#else
  return false;
#endif
}

inline bool IsChromeOS() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureX86_64() {
#if defined(__x86_64__)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureI386() {
#if defined(__i386__)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureArm() {
#if defined(__arm__)
  return true;
#else
  return false;
#endif
}

intptr_t CrashSIGSYS_Handler(const struct arch_seccomp_data& args, void* aux) {
  int syscall = args.nr;
  if (syscall >= 1024)
    syscall = 0;
  // Encode 8-bits of the 1st two arguments too, so we can discern which socket
  // type, which fcntl, ... etc., without being likely to hit a mapped
  // address.
  // Do not encode more bits here without thinking about increasing the
  // likelihood of collision with mapped pages.
  syscall |= ((args.args[0] & 0xffUL) << 12);
  syscall |= ((args.args[1] & 0xffUL) << 20);
  // Purposefully dereference the syscall as an address so it'll show up very
  // clearly and easily in crash dumps.
  volatile char* addr = reinterpret_cast<volatile char*>(syscall);
  *addr = '\0';
  // In case we hit a mapped address, hit the null page with just the syscall,
  // for paranoia.
  syscall &= 0xfffUL;
  addr = reinterpret_cast<volatile char*>(syscall);
  *addr = '\0';
  for (;;)
    _exit(1);
}

// TODO(jln): rewrite reporting functions.
intptr_t ReportCloneFailure(const struct arch_seccomp_data& args, void* aux) {
  // "flags" in the first argument in the kernel's clone().
  // Mark as volatile to be able to find the value on the stack in a minidump.
#if !defined(NDEBUG)
  RAW_LOG(ERROR, __FILE__":**CRASHING**:clone() failure\n");
#endif
  volatile uint64_t clone_flags = args.args[0];
  volatile char* addr;
  if (IsArchitectureX86_64()) {
    addr = reinterpret_cast<volatile char*>(clone_flags & 0xFFFFFF);
    *addr = '\0';
  }
  // Hit the NULL page if this fails to fault.
  addr = reinterpret_cast<volatile char*>(clone_flags & 0xFFF);
  *addr = '\0';
  for (;;)
    _exit(1);
}

// TODO(jln): rewrite reporting functions.
intptr_t ReportPrctlFailure(const struct arch_seccomp_data& args,
                            void* /* aux */) {
  // Mark as volatile to be able to find the value on the stack in a minidump.
#if !defined(NDEBUG)
  RAW_LOG(ERROR, __FILE__":**CRASHING**:prctl() failure\n");
#endif
  volatile uint64_t option = args.args[0];
  volatile char* addr =
      reinterpret_cast<volatile char*>(option & 0xFFF);
  *addr = '\0';
  for (;;)
    _exit(1);
}

intptr_t ReportIoctlFailure(const struct arch_seccomp_data& args,
                            void* /* aux */) {
  // Make "request" volatile so that we can see it on the stack in a minidump.
#if !defined(NDEBUG)
  RAW_LOG(ERROR, __FILE__":**CRASHING**:ioctl() failure\n");
#endif
  volatile uint64_t request = args.args[1];
  volatile char* addr = reinterpret_cast<volatile char*>(request & 0xFFFF);
  *addr = '\0';
  // Hit the NULL page if this fails.
  addr = reinterpret_cast<volatile char*>(request & 0xFFF);
  *addr = '\0';
  for (;;)
    _exit(1);
}

bool IsAcceleratedVideoDecodeEnabled() {
  // Accelerated video decode is currently enabled on Chrome OS,
  // but not on Linux: crbug.com/137247.
  bool is_enabled = IsChromeOS();

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  is_enabled = is_enabled &&
      !command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode);

  return is_enabled;
}

intptr_t GpuOpenSIGSYS_Handler(const struct arch_seccomp_data& args,
                               void* aux_broker_process) {
  RAW_CHECK(aux_broker_process);
  BrokerProcess* broker_process =
      static_cast<BrokerProcess*>(aux_broker_process);
  switch(args.nr) {
    case __NR_open:
      return broker_process->Open(reinterpret_cast<const char*>(args.args[0]),
                                  static_cast<int>(args.args[1]));
    case __NR_openat:
      // Allow using openat() as open().
      if (static_cast<int>(args.args[0]) == AT_FDCWD) {
        return
            broker_process->Open(reinterpret_cast<const char*>(args.args[1]),
                                 static_cast<int>(args.args[2]));
      } else {
        return -EPERM;
      }
    default:
      RAW_CHECK(false);
      return -ENOSYS;
  }
}

// The functions below cover all existing i386, x86_64, and ARM system calls;
// excluding syscalls made obsolete in ARM EABI.
// The implicitly defined sets form a partition of the sets of
// system calls.

// TODO(jln) we need to restrict the first parameter!
bool IsKill(int sysno) {
  switch (sysno) {
    case __NR_kill:
    case __NR_tkill:
    case __NR_tgkill:
      return true;
    default:
      return false;
  }
}

bool IsAllowedGettime(int sysno) {
  switch (sysno) {
    case __NR_clock_gettime:
    case __NR_gettimeofday:
#if defined(__i386__) || defined(__x86_64__)
    case __NR_time:
#endif
      return true;
    case __NR_adjtimex:         // Privileged.
    case __NR_clock_adjtime:    // Privileged.
    case __NR_clock_getres:     // Could be allowed.
    case __NR_clock_nanosleep:  // Could be allowed.
    case __NR_clock_settime:    // Privileged.
#if defined(__i386__)
    case __NR_ftime:            // Obsolete.
#endif
    case __NR_settimeofday:     // Privileged.
#if defined(__i386__)
    case __NR_stime:
#endif
    default:
      return false;
  }
}

bool IsCurrentDirectory(int sysno) {
  switch (sysno)  {
    case __NR_getcwd:
    case __NR_chdir:
    case __NR_fchdir:
      return true;
    default:
      return false;
  }
}

bool IsUmask(int sysno) {
  switch (sysno) {
    case __NR_umask:
      return true;
    default:
      return false;
  }
}

// System calls that directly access the file system. They might acquire
// a new file descriptor or otherwise perform an operation directly
// via a path.
// Both EPERM and ENOENT are valid errno unless otherwise noted in comment.
bool IsFileSystem(int sysno) {
  switch (sysno) {
    case __NR_access:          // EPERM not a valid errno.
    case __NR_chmod:
    case __NR_chown:
#if defined(__i386__) || defined(__arm__)
    case __NR_chown32:
#endif
    case __NR_creat:
    case __NR_execve:
    case __NR_faccessat:       // EPERM not a valid errno.
    case __NR_fchmodat:
    case __NR_fchownat:        // Should be called chownat ?
#if defined(__x86_64__)
    case __NR_newfstatat:      // fstatat(). EPERM not a valid errno.
#elif defined(__i386__) || defined(__arm__)
    case __NR_fstatat64:
#endif
    case __NR_futimesat:       // Should be called utimesat ?
    case __NR_lchown:
#if defined(__i386__) || defined(__arm__)
    case __NR_lchown32:
#endif
    case __NR_link:
    case __NR_linkat:
    case __NR_lookup_dcookie:  // ENOENT not a valid errno.
    case __NR_lstat:           // EPERM not a valid errno.
#if defined(__i386__)
    case __NR_oldlstat:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_lstat64:
#endif
    case __NR_mkdir:
    case __NR_mkdirat:
    case __NR_mknod:
    case __NR_mknodat:
    case __NR_open:
    case __NR_openat:
    case __NR_readlink:        // EPERM not a valid errno.
    case __NR_readlinkat:
    case __NR_rename:
    case __NR_renameat:
    case __NR_rmdir:
    case __NR_stat:            // EPERM not a valid errno.
#if defined(__i386__)
    case __NR_oldstat:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_stat64:
#endif
    case __NR_statfs:          // EPERM not a valid errno.
#if defined(__i386__) || defined(__arm__)
    case __NR_statfs64:
#endif
    case __NR_symlink:
    case __NR_symlinkat:
    case __NR_truncate:
#if defined(__i386__) || defined(__arm__)
    case __NR_truncate64:
#endif
    case __NR_unlink:
    case __NR_unlinkat:
    case __NR_uselib:          // Neither EPERM, nor ENOENT are valid errno.
    case __NR_ustat:           // Same as above. Deprecated.
#if defined(__i386__) || defined(__x86_64__)
    case __NR_utime:
#endif
    case __NR_utimensat:       // New.
    case __NR_utimes:
      return true;
    default:
      return false;
  }
}

bool IsAllowedFileSystemAccessViaFd(int sysno) {
  switch (sysno) {
    case __NR_fstat:
#if defined(__i386__) || defined(__arm__)
    case __NR_fstat64:
#endif
      return true;
    // TODO(jln): these should be denied gracefully as well (moved below).
#if defined(__i386__) || defined(__x86_64__)
    case __NR_fadvise64:        // EPERM not a valid errno.
#endif
#if defined(__i386__)
    case __NR_fadvise64_64:
#endif
#if defined(__arm__)
    case __NR_arm_fadvise64_64:
#endif
    case __NR_fdatasync:        // EPERM not a valid errno.
    case __NR_flock:            // EPERM not a valid errno.
    case __NR_fstatfs:          // Give information about the whole filesystem.
#if defined(__i386__) || defined(__arm__)
    case __NR_fstatfs64:
#endif
    case __NR_fsync:            // EPERM not a valid errno.
#if defined(__i386__)
    case __NR_oldfstat:
#endif
#if defined(__i386__) || defined(__x86_64__)
    case __NR_sync_file_range:      // EPERM not a valid errno.
#elif defined(__arm__)
    case __NR_arm_sync_file_range:  // EPERM not a valid errno.
#endif
    default:
      return false;
  }
}

// EPERM is a good errno for any of these.
bool IsDeniedFileSystemAccessViaFd(int sysno) {
  switch (sysno) {
    case __NR_fallocate:
    case __NR_fchmod:
    case __NR_fchown:
    case __NR_ftruncate:
#if defined(__i386__) || defined(__arm__)
    case __NR_fchown32:
    case __NR_ftruncate64:
#endif
    case __NR_getdents:         // EPERM not a valid errno.
    case __NR_getdents64:       // EPERM not a valid errno.
#if defined(__i386__)
    case __NR_readdir:
#endif
      return true;
    default:
      return false;
  }
}

bool IsGetSimpleId(int sysno) {
  switch (sysno) {
    case __NR_capget:
    case __NR_getegid:
    case __NR_geteuid:
    case __NR_getgid:
    case __NR_getgroups:
    case __NR_getpid:
    case __NR_getppid:
    case __NR_getresgid:
    case __NR_getsid:
    case __NR_gettid:
    case __NR_getuid:
    case __NR_getresuid:
#if defined(__i386__) || defined(__arm__)
    case __NR_getegid32:
    case __NR_geteuid32:
    case __NR_getgid32:
    case __NR_getgroups32:
    case __NR_getresgid32:
    case __NR_getresuid32:
    case __NR_getuid32:
#endif
      return true;
    default:
      return false;
  }
}

bool IsProcessPrivilegeChange(int sysno) {
  switch (sysno) {
    case __NR_capset:
#if defined(__i386__) || defined(__x86_64__)
    case __NR_ioperm:  // Intel privilege.
    case __NR_iopl:    // Intel privilege.
#endif
    case __NR_setfsgid:
    case __NR_setfsuid:
    case __NR_setgid:
    case __NR_setgroups:
    case __NR_setregid:
    case __NR_setresgid:
    case __NR_setresuid:
    case __NR_setreuid:
    case __NR_setuid:
#if defined(__i386__) || defined(__arm__)
    case __NR_setfsgid32:
    case __NR_setfsuid32:
    case __NR_setgid32:
    case __NR_setgroups32:
    case __NR_setregid32:
    case __NR_setresgid32:
    case __NR_setresuid32:
    case __NR_setreuid32:
    case __NR_setuid32:
#endif
      return true;
    default:
      return false;
  }
}

bool IsProcessGroupOrSession(int sysno) {
  switch (sysno) {
    case __NR_setpgid:
    case __NR_getpgrp:
    case __NR_setsid:
    case __NR_getpgid:
      return true;
    default:
      return false;
  }
}

bool IsAllowedSignalHandling(int sysno) {
  switch (sysno) {
    case __NR_rt_sigaction:
    case __NR_rt_sigprocmask:
    case __NR_rt_sigreturn:
#if defined(__i386__) || defined(__arm__)
    case __NR_sigaction:
    case __NR_sigprocmask:
    case __NR_sigreturn:
#endif
      return true;
    case __NR_rt_sigpending:
    case __NR_rt_sigqueueinfo:
    case __NR_rt_sigsuspend:
    case __NR_rt_sigtimedwait:
    case __NR_rt_tgsigqueueinfo:
    case __NR_sigaltstack:
    case __NR_signalfd:
    case __NR_signalfd4:
#if defined(__i386__) || defined(__arm__)
    case __NR_sigpending:
    case __NR_sigsuspend:
#endif
#if defined(__i386__)
    case __NR_signal:
    case __NR_sgetmask:  // Obsolete.
    case __NR_ssetmask:
#endif
    default:
      return false;
  }
}

bool IsOperationOnFd(int sysno) {
  switch (sysno) {
    case __NR_close:
    case __NR_dup:
    case __NR_dup2:
    case __NR_dup3:
    case __NR_fcntl:  // TODO(jln): we may want to restrict arguments.
#if defined(__i386__) || defined(__arm__)
    case __NR_fcntl64:
#endif
#if defined(__x86_64__) || defined(__arm__)
    case __NR_shutdown:
#endif
      return true;
    default:
      return false;
  }
}

bool IsKernelInternalApi(int sysno) {
  switch (sysno) {
    case __NR_restart_syscall:
#if defined(__arm__)
    case __ARM_NR_cmpxchg:
#endif
      return true;
    default:
      return false;
  }
}

// This should be thought through in conjunction with IsFutex().
bool IsAllowedProcessStartOrDeath(int sysno) {
  switch (sysno) {
    case __NR_clone:  // TODO(jln): restrict flags.
    case __NR_exit:
    case __NR_exit_group:
    case __NR_wait4:
    case __NR_waitid:
#if defined(__i386__)
    case __NR_waitpid:
#endif
      return true;
    case __NR_setns:  // Privileged.
    case __NR_fork:
#if defined(__i386__) || defined(__x86_64__)
    case __NR_get_thread_area:
    case __NR_set_thread_area:
#endif
    case __NR_set_tid_address:
    case __NR_unshare:
    case __NR_vfork:
    default:
      return false;
  }
}

// It's difficult to restrict those, but there is attack surface here.
bool IsFutex(int sysno) {
  switch (sysno) {
    case __NR_futex:
    case __NR_get_robust_list:
    case __NR_set_robust_list:
      return true;
    default:
      return false;
  }
}

bool IsAllowedEpoll(int sysno) {
  switch (sysno) {
    case __NR_epoll_create:
    case __NR_epoll_create1:
    case __NR_epoll_ctl:
    case __NR_epoll_wait:
      return true;
    default:
#if defined(__x86_64__)
    case __NR_epoll_ctl_old:
#endif
    case __NR_epoll_pwait:
#if defined(__x86_64__)
    case __NR_epoll_wait_old:
#endif
      return false;
  }
}

bool IsAllowedGetOrModifySocket(int sysno) {
  switch (sysno) {
    case __NR_pipe:
    case __NR_pipe2:
      return true;
    default:
#if defined(__x86_64__) || defined(__arm__)
    case __NR_socketpair:  // We will want to inspect its argument.
#endif
      return false;
  }
}

bool IsDeniedGetOrModifySocket(int sysno) {
  switch (sysno) {
#if defined(__x86_64__) || defined(__arm__)
    case __NR_accept:
    case __NR_accept4:
    case __NR_bind:
    case __NR_connect:
    case __NR_socket:
    case __NR_listen:
      return true;
#endif
    default:
      return false;
  }
}

#if defined(__i386__)
// Big multiplexing system call for sockets.
bool IsSocketCall(int sysno) {
  switch (sysno) {
    case __NR_socketcall:
      return true;
    default:
      return false;
  }
}
#endif

#if defined(__x86_64__) || defined(__arm__)
bool IsNetworkSocketInformation(int sysno) {
  switch (sysno) {
    case __NR_getpeername:
    case __NR_getsockname:
    case __NR_getsockopt:
    case __NR_setsockopt:
      return true;
    default:
      return false;
  }
}
#endif

bool IsAllowedAddressSpaceAccess(int sysno) {
  switch (sysno) {
    case __NR_brk:
    case __NR_madvise:
    case __NR_mlock:
#if defined(__i386__) || defined(__x86_64__)
    case __NR_mmap:   // TODO(jln): to restrict flags.
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_mmap2:
#endif
    case __NR_mprotect:
    case __NR_munlock:
    case __NR_munmap:
      return true;
    case __NR_mincore:
    case __NR_mlockall:
#if defined(__i386__) || defined(__x86_64__)
    case __NR_modify_ldt:
#endif
    case __NR_mremap:
    case __NR_msync:
    case __NR_munlockall:
    case __NR_readahead:
    case __NR_remap_file_pages:
#if defined(__i386__)
    case __NR_vm86:
    case __NR_vm86old:
#endif
    default:
      return false;
  }
}

bool IsAllowedGeneralIo(int sysno) {
  switch (sysno) {
    case __NR_lseek:
#if defined(__i386__) || defined(__arm__)
    case __NR__llseek:
#endif
    case __NR_poll:
    case __NR_ppoll:
    case __NR_pselect6:
    case __NR_read:
    case __NR_readv:
#if defined(__arm__)
    case __NR_recv:
#endif
#if defined(__x86_64__) || defined(__arm__)
    case __NR_recvfrom:  // Could specify source.
    case __NR_recvmsg:   // Could specify source.
#endif
#if defined(__i386__) || defined(__x86_64__)
    case __NR_select:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR__newselect:
#endif
#if defined(__arm__)
    case __NR_send:
#endif
#if defined(__x86_64__) || defined(__arm__)
    case __NR_sendmsg:   // Could specify destination.
    case __NR_sendto:    // Could specify destination.
#endif
    case __NR_write:
    case __NR_writev:
      return true;
    case __NR_ioctl:     // Can be very powerful.
    case __NR_pread64:
    case __NR_preadv:
    case __NR_pwrite64:
    case __NR_pwritev:
    case __NR_recvmmsg:  // Could specify source.
    case __NR_sendfile:
#if defined(__i386__) || defined(__arm__)
    case __NR_sendfile64:
#endif
    case __NR_sendmmsg:  // Could specify destination.
    case __NR_splice:
    case __NR_tee:
    case __NR_vmsplice:
    default:
      return false;
  }
}

bool IsAllowedPrctl(int sysno) {
  switch (sysno) {
    case __NR_prctl:
      return true;
    default:
#if defined(__x86_64__)
    case __NR_arch_prctl:
#endif
      return false;
  }
}

bool IsAllowedBasicScheduler(int sysno) {
  switch (sysno) {
    case __NR_sched_yield:
    case __NR_pause:
    case __NR_nanosleep:
      return true;
    case __NR_getpriority:
#if defined(__i386__) || defined(__arm__)
    case __NR_nice:
#endif
    case __NR_setpriority:
    default:
      return false;
  }
}

bool IsAdminOperation(int sysno) {
  switch (sysno) {
#if defined(__i386__) || defined(__arm__)
    case __NR_bdflush:
#endif
    case __NR_kexec_load:
    case __NR_reboot:
    case __NR_setdomainname:
    case __NR_sethostname:
    case __NR_syslog:
      return true;
    default:
      return false;
  }
}

bool IsKernelModule(int sysno) {
  switch (sysno) {
#if defined(__i386__) || defined(__x86_64__)
    case __NR_create_module:
    case __NR_get_kernel_syms:  // Should ENOSYS.
    case __NR_query_module:
#endif
    case __NR_delete_module:
    case __NR_init_module:
      return true;
    default:
      return false;
  }
}

bool IsGlobalFSViewChange(int sysno) {
  switch (sysno) {
    case __NR_pivot_root:
    case __NR_chroot:
    case __NR_sync:
      return true;
    default:
      return false;
  }
}

bool IsFsControl(int sysno) {
  switch (sysno) {
    case __NR_mount:
    case __NR_nfsservctl:
    case __NR_quotactl:
    case __NR_swapoff:
    case __NR_swapon:
#if defined(__i386__)
    case __NR_umount:
#endif
    case __NR_umount2:
      return true;
    default:
      return false;
  }
}

bool IsNuma(int sysno) {
  switch (sysno) {
    case __NR_get_mempolicy:
    case __NR_getcpu:
    case __NR_mbind:
#if defined(__i386__) || defined(__x86_64__)
    case __NR_migrate_pages:
#endif
    case __NR_move_pages:
    case __NR_set_mempolicy:
      return true;
    default:
      return false;
  }
}

bool IsMessageQueue(int sysno) {
  switch (sysno) {
    case __NR_mq_getsetattr:
    case __NR_mq_notify:
    case __NR_mq_open:
    case __NR_mq_timedreceive:
    case __NR_mq_timedsend:
    case __NR_mq_unlink:
      return true;
    default:
      return false;
  }
}

bool IsGlobalProcessEnvironment(int sysno) {
  switch (sysno) {
    case __NR_acct:         // Privileged.
#if defined(__i386__) || defined(__x86_64__)
    case __NR_getrlimit:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_ugetrlimit:
#endif
#if defined(__i386__)
    case __NR_ulimit:
#endif
    case __NR_getrusage:
    case __NR_personality:  // Can change its personality as well.
    case __NR_prlimit64:    // Like setrlimit / getrlimit.
    case __NR_setrlimit:
    case __NR_times:
      return true;
    default:
      return false;
  }
}

bool IsDebug(int sysno) {
  switch (sysno) {
    case __NR_ptrace:
    case __NR_process_vm_readv:
    case __NR_process_vm_writev:
#if defined(__i386__) || defined(__x86_64__)
    case __NR_kcmp:
#endif
      return true;
    default:
      return false;
  }
}

bool IsGlobalSystemStatus(int sysno) {
  switch (sysno) {
    case __NR__sysctl:
    case __NR_sysfs:
    case __NR_sysinfo:
    case __NR_uname:
#if defined(__i386__)
    case __NR_olduname:
    case __NR_oldolduname:
#endif
      return true;
    default:
      return false;
  }
}

bool IsEventFd(int sysno) {
  switch (sysno) {
    case __NR_eventfd:
    case __NR_eventfd2:
      return true;
    default:
      return false;
  }
}

// Asynchronous I/O API.
bool IsAsyncIo(int sysno) {
  switch (sysno) {
    case __NR_io_cancel:
    case __NR_io_destroy:
    case __NR_io_getevents:
    case __NR_io_setup:
    case __NR_io_submit:
      return true;
    default:
      return false;
  }
}

bool IsKeyManagement(int sysno) {
  switch (sysno) {
    case __NR_add_key:
    case __NR_keyctl:
    case __NR_request_key:
      return true;
    default:
      return false;
  }
}

#if defined(__x86_64__) || defined(__arm__)
bool IsSystemVSemaphores(int sysno) {
  switch (sysno) {
    case __NR_semctl:
    case __NR_semget:
    case __NR_semop:
    case __NR_semtimedop:
      return true;
    default:
      return false;
  }
}
#endif

#if defined(__x86_64__) || defined(__arm__)
// These give a lot of ambient authority and bypass the setuid sandbox.
bool IsSystemVSharedMemory(int sysno) {
  switch (sysno) {
    case __NR_shmat:
    case __NR_shmctl:
    case __NR_shmdt:
    case __NR_shmget:
      return true;
    default:
      return false;
  }
}
#endif

#if defined(__x86_64__) || defined(__arm__)
bool IsSystemVMessageQueue(int sysno) {
  switch (sysno) {
    case __NR_msgctl:
    case __NR_msgget:
    case __NR_msgrcv:
    case __NR_msgsnd:
      return true;
    default:
      return false;
  }
}
#endif

#if defined(__i386__)
// Big system V multiplexing system call.
bool IsSystemVIpc(int sysno) {
  switch (sysno) {
    case __NR_ipc:
      return true;
    default:
      return false;
  }
}
#endif

bool IsAdvancedScheduler(int sysno) {
  switch (sysno) {
    case __NR_ioprio_get:  // IO scheduler.
    case __NR_ioprio_set:
    case __NR_sched_get_priority_max:
    case __NR_sched_get_priority_min:
    case __NR_sched_getaffinity:
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_rr_get_interval:
    case __NR_sched_setaffinity:
    case __NR_sched_setparam:
    case __NR_sched_setscheduler:
      return true;
    default:
      return false;
  }
}

bool IsInotify(int sysno) {
  switch (sysno) {
    case __NR_inotify_add_watch:
    case __NR_inotify_init:
    case __NR_inotify_init1:
    case __NR_inotify_rm_watch:
      return true;
    default:
      return false;
  }
}

bool IsFaNotify(int sysno) {
  switch (sysno) {
    case __NR_fanotify_init:
    case __NR_fanotify_mark:
      return true;
    default:
      return false;
  }
}

bool IsTimer(int sysno) {
  switch (sysno) {
    case __NR_getitimer:
#if defined(__i386__) || defined(__x86_64__)
    case __NR_alarm:
#endif
    case __NR_setitimer:
      return true;
    default:
      return false;
  }
}

bool IsAdvancedTimer(int sysno) {
  switch (sysno) {
    case __NR_timer_create:
    case __NR_timer_delete:
    case __NR_timer_getoverrun:
    case __NR_timer_gettime:
    case __NR_timer_settime:
    case __NR_timerfd_create:
    case __NR_timerfd_gettime:
    case __NR_timerfd_settime:
      return true;
    default:
      return false;
  }
}

bool IsExtendedAttributes(int sysno) {
  switch (sysno) {
    case __NR_fgetxattr:
    case __NR_flistxattr:
    case __NR_fremovexattr:
    case __NR_fsetxattr:
    case __NR_getxattr:
    case __NR_lgetxattr:
    case __NR_listxattr:
    case __NR_llistxattr:
    case __NR_lremovexattr:
    case __NR_lsetxattr:
    case __NR_removexattr:
    case __NR_setxattr:
      return true;
    default:
      return false;
  }
}

// Various system calls that need to be researched.
// TODO(jln): classify this better.
bool IsMisc(int sysno) {
  switch (sysno) {
    case __NR_name_to_handle_at:
    case __NR_open_by_handle_at:
    case __NR_perf_event_open:
    case __NR_syncfs:
    case __NR_vhangup:
    // The system calls below are not implemented.
#if defined(__i386__) || defined(__x86_64__)
    case __NR_afs_syscall:
#endif
#if defined(__i386__)
    case __NR_break:
#endif
#if defined(__i386__) || defined(__x86_64__)
    case __NR_getpmsg:
#endif
#if defined(__i386__)
    case __NR_gtty:
    case __NR_idle:
    case __NR_lock:
    case __NR_mpx:
    case __NR_prof:
    case __NR_profil:
#endif
#if defined(__i386__) || defined(__x86_64__)
    case __NR_putpmsg:
#endif
#if defined(__x86_64__)
    case __NR_security:
#endif
#if defined(__i386__)
    case __NR_stty:
#endif
#if defined(__x86_64__)
    case __NR_tuxcall:
#endif
    case __NR_vserver:
      return true;
    default:
      return false;
  }
}

#if defined(__arm__)
bool IsArmPciConfig(int sysno) {
  switch (sysno) {
    case __NR_pciconfig_iobase:
    case __NR_pciconfig_read:
    case __NR_pciconfig_write:
      return true;
    default:
      return false;
  }
}

bool IsArmPrivate(int sysno) {
  switch (sysno) {
    case __ARM_NR_breakpoint:
    case __ARM_NR_cacheflush:
    case __ARM_NR_set_tls:
    case __ARM_NR_usr26:
    case __ARM_NR_usr32:
      return true;
    default:
      return false;
  }
}
#endif  // defined(__arm__)

// End of the system call sets section.

bool IsBaselinePolicyAllowed(int sysno) {
  if (IsAllowedAddressSpaceAccess(sysno) ||
      IsAllowedBasicScheduler(sysno) ||
      IsAllowedEpoll(sysno) ||
      IsAllowedFileSystemAccessViaFd(sysno) ||
      IsAllowedGeneralIo(sysno) ||
      IsAllowedGetOrModifySocket(sysno) ||
      IsAllowedGettime(sysno) ||
      IsAllowedPrctl(sysno) ||
      IsAllowedProcessStartOrDeath(sysno) ||
      IsAllowedSignalHandling(sysno) ||
      IsFutex(sysno) ||
      IsGetSimpleId(sysno) ||
      IsKernelInternalApi(sysno) ||
#if defined(__arm__)
      IsArmPrivate(sysno) ||
#endif
      IsKill(sysno) ||
      IsOperationOnFd(sysno)) {
    return true;
  } else {
    return false;
  }
}

// System calls that will trigger the crashing SIGSYS handler.
bool IsBaselinePolicyWatched(int sysno) {
  if (IsAdminOperation(sysno) ||
      IsAdvancedScheduler(sysno) ||
      IsAdvancedTimer(sysno) ||
      IsAsyncIo(sysno) ||
      IsDebug(sysno) ||
      IsEventFd(sysno) ||
      IsExtendedAttributes(sysno) ||
      IsFaNotify(sysno) ||
      IsFsControl(sysno) ||
      IsGlobalFSViewChange(sysno) ||
      IsGlobalProcessEnvironment(sysno) ||
      IsGlobalSystemStatus(sysno) ||
      IsInotify(sysno) ||
      IsKernelModule(sysno) ||
      IsKeyManagement(sysno) ||
      IsMessageQueue(sysno) ||
      IsMisc(sysno) ||
#if defined(__x86_64__)
      IsNetworkSocketInformation(sysno) ||
#endif
      IsNuma(sysno) ||
      IsProcessGroupOrSession(sysno) ||
      IsProcessPrivilegeChange(sysno) ||
#if defined(__i386__)
      IsSocketCall(sysno) ||  // We'll need to handle this properly to build
                              // a x86_32 policy.
#endif
#if defined(__x86_64__) || defined(__arm__)
      IsSystemVMessageQueue(sysno) ||
      IsSystemVSemaphores(sysno) ||
      IsSystemVSharedMemory(sysno) ||
#elif defined(__i386__)
      IsSystemVIpc(sysno) ||
#endif
#if defined(__arm__)
      IsArmPciConfig(sysno) ||
#endif
      IsTimer(sysno)) {
    return true;
  } else {
    return false;
  }
}

ErrorCode BaselinePolicy(Sandbox *sandbox, int sysno) {
#if defined(__x86_64__) || defined(__arm__)
  if (sysno == __NR_socketpair) {
    // Only allow AF_UNIX, PF_UNIX. Crash if anything else is seen.
    COMPILE_ASSERT(AF_UNIX == PF_UNIX, af_unix_pf_unix_different);
    return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL, AF_UNIX,
                         ErrorCode(ErrorCode::ERR_ALLOWED),
                         sandbox->Trap(CrashSIGSYS_Handler, NULL));
  }
#endif
  if (IsBaselinePolicyAllowed(sysno)) {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }

#if defined(__i386__)
  // socketcall(2) should be tightened.
  if (IsSocketCall(sysno)) {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
#endif

  // TODO(jln): some system calls in those sets are not supposed to
  // return ENOENT. Return the appropriate error.
  if (IsFileSystem(sysno) || IsCurrentDirectory(sysno)) {
    return ErrorCode(ENOENT);
  }

  if (IsUmask(sysno) || IsDeniedFileSystemAccessViaFd(sysno) ||
      IsDeniedGetOrModifySocket(sysno)) {
    return ErrorCode(EPERM);
  }

  if (IsBaselinePolicyWatched(sysno)) {
    // Previously unseen syscalls. TODO(jln): some of these should
    // be denied gracefully right away.
    return sandbox->Trap(CrashSIGSYS_Handler, NULL);
  }
  // In any other case crash the program with our SIGSYS handler
  return sandbox->Trap(CrashSIGSYS_Handler, NULL);
}

// x86_64/i386 for now. Needs to be adapted and tested for ARM.
ErrorCode GpuProcessPolicy(Sandbox *sandbox, int sysno,
                           void *broker_process) {
  switch(sysno) {
    case __NR_ioctl:
#if defined(ADDRESS_SANITIZER)
    // Allow to call sched_getaffinity under AddressSanitizer.
    case __NR_sched_getaffinity:
#endif
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    case __NR_open:
    case __NR_openat:
        return sandbox->Trap(GpuOpenSIGSYS_Handler, broker_process);
    default:
#if defined(__x86_64__) || defined(__arm__)
      if (IsSystemVSharedMemory(sysno))
        return ErrorCode(EACCES);
#endif
      if (IsEventFd(sysno))
        return ErrorCode(ErrorCode::ERR_ALLOWED);

      // Default on the baseline policy.
      return BaselinePolicy(sandbox, sysno);
  }
}

// x86_64/i386 for now. Needs to be adapted and tested for ARM.
// A GPU broker policy is the same as a GPU policy with open and
// openat allowed.
ErrorCode GpuBrokerProcessPolicy(Sandbox *sandbox, int sysno, void *aux) {
  // "aux" would typically be NULL, when called from
  // "EnableGpuBrokerPolicyCallBack"
  switch(sysno) {
    case __NR_open:
    case __NR_openat:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    default:
      return GpuProcessPolicy(sandbox, sysno, aux);
  }
}

// Allow clone for threads, crash if anything else is attempted.
// Don't restrict on ASAN.
ErrorCode RestrictCloneToThreads(Sandbox *sandbox) {
  // Glibc's pthread.
  if (!RunningOnASAN()) {
    return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
        CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
        CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS |
        CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID,
        ErrorCode(ErrorCode::ERR_ALLOWED),
        sandbox->Trap(ReportCloneFailure, NULL));
  } else {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

ErrorCode RestrictPrctl(Sandbox *sandbox) {
  // Allow PR_SET_NAME, PR_SET_DUMPABLE, PR_GET_DUMPABLE. Will need to add
  // seccomp compositing in the future.
  // PR_SET_PTRACER is used by breakpad but not needed anymore.
  return sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       PR_SET_NAME, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       PR_SET_DUMPABLE, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                       PR_GET_DUMPABLE, ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Trap(ReportPrctlFailure, NULL))));
}

ErrorCode RestrictIoctl(Sandbox *sandbox) {
  // Allow TCGETS and FIONREAD, trap to ReportIoctlFailure otherwise.
  return sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_EQUAL, TCGETS,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
         sandbox->Cond(1, ErrorCode::TP_64BIT, ErrorCode::OP_EQUAL, FIONREAD,
                       ErrorCode(ErrorCode::ERR_ALLOWED),
                       sandbox->Trap(ReportIoctlFailure, NULL)));
}

ErrorCode RendererOrWorkerProcessPolicy(Sandbox *sandbox, int sysno, void *) {
  switch (sysno) {
    case __NR_clone:
      return RestrictCloneToThreads(sandbox);
    case __NR_ioctl:
      // Restrict IOCTL on x86_64.
      if (IsArchitectureX86_64()) {
        return RestrictIoctl(sandbox);
      } else {
        return ErrorCode(ErrorCode::ERR_ALLOWED);
      }
    case __NR_prctl:
      return RestrictPrctl(sandbox);
    // Allow the system calls below.
    case __NR_fdatasync:
    case __NR_fsync:
#if defined(__i386__) || defined(__x86_64__)
    case __NR_getrlimit:
#endif
    case __NR_mremap:   // See crbug.com/149834.
    case __NR_pread64:
    case __NR_pwrite64:
#if defined(ADDRESS_SANITIZER)
    // Allow to call sched_getaffinity() under AddressSanitizer.
    case __NR_sched_getaffinity:
#endif
    case __NR_sched_get_priority_max:
    case __NR_sched_get_priority_min:
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_setscheduler:
    case __NR_setpriority:
    case __NR_sysinfo:
    case __NR_times:
    case __NR_uname:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    case __NR_prlimit64:
      return ErrorCode(EPERM);  // See crbug.com/160157.
    default:
      // These need further tightening.
#if defined(__x86_64__) || defined(__arm__)
      if (IsSystemVSharedMemory(sysno))
        return ErrorCode(ErrorCode::ERR_ALLOWED);
#endif
#if defined(__i386__)
      if (IsSystemVIpc(sysno))
        return ErrorCode(ErrorCode::ERR_ALLOWED);
#endif

      // Default on the baseline policy.
      return BaselinePolicy(sandbox, sysno);
  }
}

ErrorCode FlashProcessPolicy(Sandbox *sandbox, int sysno, void *) {
  switch (sysno) {
    case __NR_sched_getaffinity:
    case __NR_sched_setscheduler:
    case __NR_times:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    case __NR_ioctl:
      return ErrorCode(ENOTTY);  // Flash Access.
    default:
      // These need further tightening.
#if defined(__x86_64__) || defined(__arm__)
      if (IsSystemVSharedMemory(sysno))
        return ErrorCode(ErrorCode::ERR_ALLOWED);
#endif
#if defined(__i386__)
      if (IsSystemVIpc(sysno))
        return ErrorCode(ErrorCode::ERR_ALLOWED);
#endif

      // Default on the baseline policy.
      return BaselinePolicy(sandbox, sysno);
  }
}

ErrorCode BlacklistDebugAndNumaPolicy(Sandbox *sandbox, int sysno, void *) {
  if (!Sandbox::IsValidSyscallNumber(sysno)) {
    // TODO(jln) we should not have to do that in a trivial policy.
    return ErrorCode(ENOSYS);
  }

  if (IsDebug(sysno) || IsNuma(sysno))
    return sandbox->Trap(CrashSIGSYS_Handler, NULL);

  return ErrorCode(ErrorCode::ERR_ALLOWED);
}

// Allow all syscalls.
// This will still deny x32 or IA32 calls in 64 bits mode or
// 64 bits system calls in compatibility mode.
ErrorCode AllowAllPolicy(Sandbox *, int sysno, void *) {
  if (!Sandbox::IsValidSyscallNumber(sysno)) {
    // TODO(jln) we should not have to do that in a trivial policy.
    return ErrorCode(ENOSYS);
  } else {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
}

bool EnableGpuBrokerPolicyCallBack() {
  StartSandboxWithPolicy(GpuBrokerProcessPolicy, NULL);
  return true;
}

// Start a broker process to handle open() inside the sandbox.
void InitGpuBrokerProcess(BrokerProcess** broker_process) {
  static const char kDriRcPath[] = "/etc/drirc";
  static const char kDriCard0Path[] = "/dev/dri/card0";

  CHECK(broker_process);
  CHECK(*broker_process == NULL);

  std::vector<std::string> read_whitelist;
  read_whitelist.push_back(kDriCard0Path);
  read_whitelist.push_back(kDriRcPath);
  std::vector<std::string> write_whitelist;
  write_whitelist.push_back(kDriCard0Path);

  *broker_process = new BrokerProcess(read_whitelist, write_whitelist);
  // Initialize the broker process and give it a sandbox call back.
  CHECK((*broker_process)->Init(EnableGpuBrokerPolicyCallBack));
}

// Warms up/preloads resources needed by the policies.
// Eventually start a broker process and return it in broker_process.
void WarmupPolicy(Sandbox::EvaluateSyscall policy,
                  BrokerProcess** broker_process) {
  if (policy == GpuProcessPolicy) {
    if (IsArchitectureX86_64() || IsArchitectureI386()) {
      // Create a new broker process.
      InitGpuBrokerProcess(broker_process);

      // Accelerated video decode dlopen()'s a shared object
      // inside the sandbox, so preload it now.
      if (IsAcceleratedVideoDecodeEnabled()) {
        const char* I965DrvVideoPath = NULL;

        if (IsArchitectureX86_64()) {
          I965DrvVideoPath = "/usr/lib64/va/drivers/i965_drv_video.so";
        } else if (IsArchitectureI386()) {
          I965DrvVideoPath = "/usr/lib/va/drivers/i965_drv_video.so";
        }

        dlopen(I965DrvVideoPath, RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
      }
    }
  }
}

Sandbox::EvaluateSyscall GetProcessSyscallPolicy(
    const CommandLine& command_line,
    const std::string& process_type) {
  if (process_type == switches::kGpuProcess) {
    // On Chrome OS, --enable-gpu-sandbox enables the more restrictive policy.
    // However, we don't yet enable the more restrictive GPU process policy
    // on ARM.
    if (IsArchitectureArm() ||
        (IsChromeOS() && !command_line.HasSwitch(switches::kEnableGpuSandbox)))
      return BlacklistDebugAndNumaPolicy;
    else
      return GpuProcessPolicy;
  }

  if (process_type == switches::kPpapiPluginProcess) {
    // TODO(jln): figure out what to do with non-Flash PPAPI
    // out-of-process plug-ins.
    return FlashProcessPolicy;
  }

  if (process_type == switches::kRendererProcess ||
      process_type == switches::kWorkerProcess) {
    return RendererOrWorkerProcessPolicy;
  }

  if (process_type == switches::kUtilityProcess) {
    return BlacklistDebugAndNumaPolicy;
  }

  NOTREACHED();
  // This will be our default if we need one.
  return AllowAllPolicy;
}

// broker_process can be NULL if there is no need for one.
void StartSandboxWithPolicy(Sandbox::EvaluateSyscall syscall_policy,
                            BrokerProcess* broker_process) {
  // Starting the sandbox is a one-way operation. The kernel doesn't allow
  // us to unload a sandbox policy after it has been started. Nonetheless,
  // in order to make the use of the "Sandbox" object easier, we allow for
  // the object to be destroyed after the sandbox has been started. Note that
  // doing so does not stop the sandbox.
  Sandbox sandbox;
  sandbox.SetSandboxPolicy(syscall_policy, broker_process);
  sandbox.StartSandbox();
}

// Initialize the seccomp-bpf sandbox.
bool StartBpfSandbox(const CommandLine& command_line,
                     const std::string& process_type) {
  Sandbox::EvaluateSyscall syscall_policy =
      GetProcessSyscallPolicy(command_line, process_type);

  BrokerProcess* broker_process = NULL;
  // Warm up resources needed by the policy we're about to enable and
  // eventually start a broker process.
  WarmupPolicy(syscall_policy, &broker_process);

  StartSandboxWithPolicy(syscall_policy, broker_process);

  return true;
}

}  // namespace

#endif  // SECCOMP_BPF_SANDBOX

namespace content {

// Is seccomp BPF globally enabled?
bool SandboxSeccompBpf::IsSeccompBpfDesired() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kNoSandbox) &&
      !command_line.HasSwitch(switches::kDisableSeccompFilterSandbox)) {
    return true;
  } else {
    return false;
  }
}

bool SandboxSeccompBpf::ShouldEnableSeccompBpf(
    const std::string& process_type) {
#if defined(SECCOMP_BPF_SANDBOX)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (process_type == switches::kGpuProcess)
    return !command_line.HasSwitch(switches::kDisableGpuSandbox);

  return true;
#endif  // SECCOMP_BPF_SANDBOX
  return false;
}

bool SandboxSeccompBpf::SupportsSandbox() {
#if defined(SECCOMP_BPF_SANDBOX)
  // TODO(jln): pass the saved proc_fd_ from the LinuxSandbox singleton
  // here.
  Sandbox::SandboxStatus bpf_sandbox_status =
      Sandbox::SupportsSeccompSandbox(-1);
  // Kernel support is what we are interested in here. Other status
  // such as STATUS_UNAVAILABLE (has threads) still indicate kernel support.
  // We make this a negative check, since if there is a bug, we would rather
  // "fail closed" (expect a sandbox to be available and try to start it).
  if (bpf_sandbox_status != Sandbox::STATUS_UNSUPPORTED) {
    return true;
  }
#endif
  return false;
}

bool SandboxSeccompBpf::StartSandbox(const std::string& process_type) {
#if defined(SECCOMP_BPF_SANDBOX)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (IsSeccompBpfDesired() &&  // Global switches policy.
      ShouldEnableSeccompBpf(process_type) &&  // Process-specific policy.
      SupportsSandbox()) {
    // If the kernel supports the sandbox, and if the command line says we
    // should enable it, enable it or die.
    bool started_sandbox = StartBpfSandbox(command_line, process_type);
    CHECK(started_sandbox);
    return true;
  }
#endif
  return false;
}

}  // namespace content
