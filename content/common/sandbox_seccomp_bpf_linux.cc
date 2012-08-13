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
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ucontext.h>
#include <unistd.h>

#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "content/common/sandbox_linux.h"
#include "content/common/sandbox_seccomp_bpf_linux.h"
#include "content/public/common/content_switches.h"

// These are the only architectures supported for now.
#if defined(__i386__) || defined(__x86_64__)
#define SECCOMP_BPF_SANDBOX
#endif

#if defined(SECCOMP_BPF_SANDBOX)
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/x86_linux_syscalls.h"

namespace {

inline bool IsChromeOS() {
#if defined(OS_CHROMEOS)
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

bool IsAcceleratedVideoDecodeEnabled() {
  // Accelerated video decode is currently enabled on Chrome OS,
  // but not on Linux: crbug.com/137247.
  bool is_enabled = IsChromeOS();

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  is_enabled = is_enabled &&
      !command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode);

  return is_enabled;
}

static const char kDriRcPath[] = "/etc/drirc";

// TODO(jorgelo): limited to /etc/drirc for now, extend this to cover
// other sandboxed file access cases.
int OpenWithCache(const char* pathname, int flags) {
  static int drircfd = -1;
  static bool do_open = true;
  int res = -1;

  if (strcmp(pathname, kDriRcPath) == 0 && flags == O_RDONLY) {
    if (do_open) {
      drircfd = open(pathname, flags);
      do_open = false;
      res = drircfd;
    } else {
      // dup() man page:
      // "After a successful return from one of these system calls,
      // the old and new file descriptors may be used interchangeably.
      // They refer to the same open file description and thus share
      // file offset and file status flags; for example, if the file offset
      // is modified by using lseek(2) on one of the descriptors,
      // the offset is also changed for the other."
      // Since |drircfd| can be dup()'ed and read many times, we need to
      // lseek() it to the beginning of the file before returning.
      // We assume the caller will not keep more than one fd open at any
      // one time. Intel driver code in Mesa that parses /etc/drirc does
      // open()/read()/close() in the same function.
      if (drircfd < 0) {
        errno = ENOENT;
        return -1;
      }
      int newfd = dup(drircfd);
      if (newfd < 0) {
        errno = ENOMEM;
        return -1;
      }
      if (lseek(newfd, 0, SEEK_SET) == static_cast<off_t>(-1)) {
        (void) HANDLE_EINTR(close(newfd));
        errno = ENOMEM;
        return -1;
      }
      res = newfd;
    }
  } else {
    res = open(pathname, flags);
  }

  return res;
}

// We allow the GPU process to open /etc/drirc because it's needed by Mesa.
// OpenWithCache() has been called before enabling the sandbox, and has cached
// a file descriptor for /etc/drirc.
intptr_t GpuOpenSIGSYS_Handler(const struct arch_seccomp_data& args,
                               void* aux) {
  uint64_t arg0 = args.args[0];
  uint64_t arg1 = args.args[1];
  const char* pathname = reinterpret_cast<const char*>(arg0);
  int flags = static_cast<int>(arg1);

  if (strcmp(pathname, kDriRcPath) == 0) {
    int ret = OpenWithCache(pathname, flags);
    return (ret == -1) ? -errno : ret;
  } else {
    return -ENOENT;
  }
}

#if defined(__x86_64__)

// The functions below cover all existing x86_64 system calls.
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
    case __NR_time:
      return true;
    case __NR_adjtimex:         // Privileged.
    case __NR_clock_adjtime:    // Privileged.
    case __NR_clock_getres:     // Could be allowed.
    case __NR_clock_nanosleep:  // Could be allowed.
    case __NR_clock_settime:    // Privileged.
    case __NR_settimeofday:     // Privileged.
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
    case __NR_creat:
    case __NR_execve:
    case __NR_faccessat:       // EPERM not a valid errno.
    case __NR_fchmodat:
    case __NR_fchownat:        // Should be called chownat ?
    case __NR_futimesat:       // Should be called utimesat ?
    case __NR_lchown:
    case __NR_link:
    case __NR_linkat:
    case __NR_lookup_dcookie:  // ENOENT not a valid errno.
    case __NR_lstat:           // EPERM not a valid errno.
    case __NR_mkdir:
    case __NR_mkdirat:
    case __NR_mknod:
    case __NR_mknodat:
    case __NR_newfstatat:      // EPERM not a valid errno.
                               // Should be called statat ?
    case __NR_open:
    case __NR_openat:
    case __NR_readlink:        // EPERM not a valid errno.
    case __NR_readlinkat:
    case __NR_rename:
    case __NR_renameat:
    case __NR_rmdir:
    case __NR_stat:            // EPERM not a valid errno.
    case __NR_statfs:          // EPERM not a valid errno.
    case __NR_symlink:
    case __NR_symlinkat:
    case __NR_truncate:
    case __NR_unlink:
    case __NR_unlinkat:
    case __NR_uselib:          // Neither EPERM, nor ENOENT are valid errno.
    case __NR_ustat:           // Same as above. Deprecated.
    case __NR_utime:
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
      return true;
    // TODO(jln): these should be denied gracefully as well (moved below).
    case __NR_fadvise64:        // EPERM not a valid errno.
    case __NR_fdatasync:        // EPERM not a valid errno.
    case __NR_flock:            // EPERM not a valid errno.
    case __NR_fstatfs:          // Give information about the whole filesystem.
    case __NR_fsync:            // EPERM not a valid errno.
    case __NR_sync_file_range:  // EPERM not a valid errno.
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
    case __NR_getdents:         // EPERM not a valid errno.
    case __NR_getdents64:       // EPERM not a valid errno.
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
    case __NR_getresuid:
    case __NR_getsid:
    case __NR_gettid:
    case __NR_getuid:
      return true;
    default:
      return false;
  }
}

bool IsProcessPrivilegeChange(int sysno) {
  switch (sysno) {
    case __NR_capset:
    case __NR_ioperm:  // Intel privilege.
    case __NR_iopl:    // Intel privilege.
    case __NR_setfsgid:
    case __NR_setfsuid:
    case __NR_setgid:
    case __NR_setgroups:
    case __NR_setregid:
    case __NR_setresgid:
    case __NR_setresuid:
    case __NR_setreuid:
    case __NR_setuid:
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
      return true;
    case __NR_rt_sigpending:
    case __NR_rt_sigqueueinfo:
    case __NR_rt_sigsuspend:
    case __NR_rt_sigtimedwait:
    case __NR_rt_tgsigqueueinfo:
    case __NR_sigaltstack:
    case __NR_signalfd:
    case __NR_signalfd4:
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
    case __NR_shutdown:
      return true;
    default:
      return false;
  }
}

bool IsKernelInteralApi(int sysno) {
  switch (sysno) {
    case __NR_restart_syscall:
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
      return true;
    case __NR_setns:  // Privileged.
    case __NR_fork:
    case __NR_get_thread_area:
    case __NR_set_thread_area:
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
    case __NR_epoll_ctl_old:
    case __NR_epoll_pwait:
    case __NR_epoll_wait_old:
      return false;
  }
}

bool IsAllowedGetOrModifySocket(int sysno) {
  switch (sysno) {
    case __NR_pipe:
    case __NR_pipe2:
    case __NR_socketpair:  // We will want to inspect its argument.
      return true;
    default:
    case __NR_accept:
    case __NR_accept4:
    case __NR_bind:
    case __NR_connect:
    case __NR_socket:
    case __NR_listen:
      return false;
  }
}

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

bool IsAllowedAddressSpaceAccess(int sysno) {
  switch (sysno) {
    case __NR_brk:
    case __NR_madvise:
    case __NR_mlock:
    case __NR_mmap:  // TODO(jln): to restrict flags.
    case __NR_mprotect:
    case __NR_munlock:
    case __NR_munmap:
      return true;
    case __NR_mincore:
    case __NR_mlockall:
    case __NR_modify_ldt:
    case __NR_mremap:
    case __NR_msync:
    case __NR_munlockall:
    case __NR_readahead:
    case __NR_remap_file_pages:
    default:
      return false;
  }
}

bool IsAllowedGeneralIo(int sysno) {
  switch (sysno) {
    case __NR_lseek:
    case __NR_poll:
    case __NR_ppoll:
    case __NR_pselect6:
    case __NR_read:
    case __NR_readv:
    case __NR_recvfrom:  // Could specify source.
    case __NR_recvmsg:   // Could specify source.
    case __NR_select:
    case __NR_sendmsg:   // Could specify destination.
    case __NR_sendto:    // Could specify destination.
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
    case __NR_arch_prctl:
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
    case __NR_setpriority:
    default:
      return false;
  }
}

bool IsAdminOperation(int sysno) {
  switch (sysno) {
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
    case __NR_create_module:
    case __NR_delete_module:
    case __NR_get_kernel_syms:  // Should ENOSYS.
    case __NR_init_module:
    case __NR_query_module:
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
    case __NR_migrate_pages:
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
    case __NR_getrlimit:
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
    case __NR_kcmp:
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

// These give a lot of ambient authority and bypass the setuid sandbox.
bool IsAllowedSystemVSharedMemory(int sysno) {
  switch (sysno) {
    case __NR_shmat:
    case __NR_shmctl:
    case __NR_shmdt:
      return true;
    case __NR_shmget:
    default:
      return false;
  }
}

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
    case __NR_alarm:
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
    case __NR_afs_syscall:
    case __NR_getpmsg:
    case __NR_putpmsg:
    case __NR_security:
    case __NR_tuxcall:
    case __NR_vserver:
      return true;
    default:
      return false;
  }
}

// End of the system call sets section.

// x86_64 only because it references system calls that are multiplexed on IA32.
bool IsBaselinePolicyAllowed_x86_64(int sysno) {
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
      IsKernelInteralApi(sysno) ||
      IsKill(sysno) ||
      IsOperationOnFd(sysno)) {
    return true;
  } else {
    return false;
  }
}

// System calls that will trigger the crashing sigsys handler.
bool IsBaselinePolicyWatched_x86_64(int sysno) {
  if (IsAdminOperation(sysno) ||
      IsAdvancedScheduler(sysno) ||
      IsAdvancedTimer(sysno) ||
      IsAllowedSystemVSharedMemory(sysno) ||
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
      IsNetworkSocketInformation(sysno) ||
      IsNuma(sysno) ||
      IsProcessGroupOrSession(sysno) ||
      IsProcessPrivilegeChange(sysno) ||
      IsSystemVMessageQueue(sysno) ||
      IsSystemVSemaphores(sysno) ||
      IsTimer(sysno)) {
    return true;
  } else {
    return false;
  }
}

playground2::Sandbox::ErrorCode BaselinePolicy_x86_64(int sysno) {
  if (IsBaselinePolicyAllowed_x86_64(sysno)) {
    return playground2::Sandbox::SB_ALLOWED;
  }
  // TODO(jln): some system calls in those sets are not supposed to
  // return ENOENT. Return the appropriate error.
  if (IsFileSystem(sysno) || IsCurrentDirectory(sysno)) {
    return ENOENT;
  }

  if (IsUmask(sysno) || IsDeniedFileSystemAccessViaFd(sysno)) {
    return EPERM;
  }

  if (IsBaselinePolicyWatched_x86_64(sysno)) {
    // Previously unseen syscalls. TODO(jln): some of these should
    // be denied gracefully right away.
    return playground2::Sandbox::ErrorCode(CrashSIGSYS_Handler, NULL);
  }
  // In any other case crash the program with our SIGSYS handler
  return playground2::Sandbox::ErrorCode(CrashSIGSYS_Handler, NULL);
}

// x86_64 only because it references system calls that are multiplexed on IA32.
playground2::Sandbox::ErrorCode GpuProcessPolicy_x86_64(int sysno) {
  switch(sysno) {
    case __NR_ioctl:
      return playground2::Sandbox::SB_ALLOWED;
    case __NR_socket:
      return EACCES;  // Nvidia binary driver.
    case __NR_open:
      // Accelerated video decode is enabled by default only on Chrome OS.
      if (IsAcceleratedVideoDecodeEnabled()) {
        // Accelerated video decode needs to open /dev/dri/card0, and
        // dup()'ing an already open file descriptor does not work.
        // Allow open() even though it severely weakens the sandbox,
        // to test the sandboxing mechanism in general.
        // TODO(jorgelo): remove this once we solve the libva issue.
        return playground2::Sandbox::SB_ALLOWED;
      } else {
        // Hook open() in the GPU process to allow opening /etc/drirc,
        // needed by Mesa.
        // The hook needs dup(), lseek(), and close() to be allowed.
        return playground2::Sandbox::ErrorCode(GpuOpenSIGSYS_Handler, NULL);
      }
    default:
      if (IsEventFd(sysno))
        return playground2::Sandbox::SB_ALLOWED;

      // Default on the baseline policy.
      return BaselinePolicy_x86_64(sysno);
  }
}

// x86_64 only because it references system calls that are multiplexed on IA32.
playground2::Sandbox::ErrorCode FlashProcessPolicy_x86_64(int sysno) {
  switch (sysno) {
    case __NR_sched_getaffinity:
    case __NR_sched_setscheduler:
    case __NR_times:
      return playground2::Sandbox::SB_ALLOWED;
    case __NR_ioctl:
      return ENOTTY;  // Flash Access.
    case __NR_socket:
      return EACCES;
    default:
      // These are under investigation, and hopefully not here for the long
      // term.
      if (IsAllowedSystemVSharedMemory(sysno))
        return playground2::Sandbox::SB_ALLOWED;

      // Default on the baseline policy.
      return  BaselinePolicy_x86_64(sysno);
  }
}
#endif

playground2::Sandbox::ErrorCode BlacklistPtracePolicy(int sysno) {
  if (sysno < static_cast<int>(MIN_SYSCALL) ||
      sysno > static_cast<int>(MAX_SYSCALL)) {
    // TODO(jln) we should not have to do that in a trivial policy.
    return ENOSYS;
  }
  switch (sysno) {
    case __NR_migrate_pages:
    case __NR_move_pages:
    case __NR_process_vm_readv:
    case __NR_process_vm_writev:
    case __NR_ptrace:
      return playground2::Sandbox::ErrorCode(CrashSIGSYS_Handler, NULL);
    default:
      return playground2::Sandbox::SB_ALLOWED;
  }
}

// Allow all syscalls.
// This will still deny x32 or IA32 calls in 64 bits mode or
// 64 bits system calls in compatibility mode.
playground2::Sandbox::ErrorCode AllowAllPolicy(int sysno) {
  if (sysno < static_cast<int>(MIN_SYSCALL) ||
      sysno > static_cast<int>(MAX_SYSCALL)) {
    // TODO(jln) we should not have to do that in a trivial policy.
    return ENOSYS;
  } else {
    return playground2::Sandbox::SB_ALLOWED;
  }
}

// Warms up/preloads resources needed by the policies.
void WarmupPolicy(playground2::Sandbox::EvaluateSyscall policy) {
#if defined(__x86_64__)
  if (policy == GpuProcessPolicy_x86_64) {
    OpenWithCache(kDriRcPath, O_RDONLY);
    // Accelerated video decode dlopen()'s this shared object
    // inside the sandbox, so preload it now.
    // TODO(jorgelo): generalize this to other platforms.
    if (IsAcceleratedVideoDecodeEnabled()) {
      const char kI965DrvVideoPath_64[] =
          "/usr/lib64/va/drivers/i965_drv_video.so";
      dlopen(kI965DrvVideoPath_64, RTLD_NOW|RTLD_GLOBAL|RTLD_NODELETE);
    }
  }
#endif
}

playground2::Sandbox::EvaluateSyscall GetProcessSyscallPolicy(
    const CommandLine& command_line,
    const std::string& process_type) {
#if defined(__x86_64__)
  if (process_type == switches::kGpuProcess) {
    // On Chrome OS, --enable-gpu-sandbox enables the more restrictive policy.
    if (IsChromeOS() && !command_line.HasSwitch(switches::kEnableGpuSandbox))
      return BlacklistPtracePolicy;
    else
      return GpuProcessPolicy_x86_64;
  }

  if (process_type == switches::kPpapiPluginProcess) {
    // TODO(jln): figure out what to do with non-Flash PPAPI
    // out-of-process plug-ins.
    return FlashProcessPolicy_x86_64;
  }

  if (process_type == switches::kRendererProcess ||
      process_type == switches::kWorkerProcess) {
    return BlacklistPtracePolicy;
  }
  NOTREACHED();
  // This will be our default if we need one.
  return AllowAllPolicy;
#else
  // On IA32, we only have a small blacklist at the moment.
  (void) process_type;
  return BlacklistPtracePolicy;
#endif  // __x86_64__
}

// Initialize the seccomp-bpf sandbox.
bool StartBpfSandbox_x86(const CommandLine& command_line,
                         const std::string& process_type) {
  playground2::Sandbox::EvaluateSyscall SyscallPolicy =
      GetProcessSyscallPolicy(command_line, process_type);

  // Warms up resources needed by the policy we're about to enable.
  WarmupPolicy(SyscallPolicy);

  playground2::Sandbox::setSandboxPolicy(SyscallPolicy, NULL);
  playground2::Sandbox::startSandbox();

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
#endif
  return false;
}

bool SandboxSeccompBpf::SupportsSandbox() {
#if defined(SECCOMP_BPF_SANDBOX)
  // TODO(jln): pass the saved proc_fd_ from the LinuxSandbox singleton
  // here.
  if (playground2::Sandbox::supportsSeccompSandbox(-1) ==
      playground2::Sandbox::STATUS_AVAILABLE) {
    return true;
  }
#endif
  return false;
}

bool SandboxSeccompBpf::StartSandbox(const std::string& process_type) {
#if defined(SECCOMP_BPF_SANDBOX)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (IsSeccompBpfDesired() &&  // Global switches policy.
      // Process-specific policy.
      ShouldEnableSeccompBpf(process_type) &&
      SupportsSandbox()) {
    return StartBpfSandbox_x86(command_line, process_type);
  }
#endif
  return false;
}

}  // namespace content
