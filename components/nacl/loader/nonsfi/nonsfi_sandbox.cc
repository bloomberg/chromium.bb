// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nonsfi/nonsfi_sandbox.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/net.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/common/sandbox_init.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"
#include "sandbox/linux/seccomp-bpf/trap.h"
#include "sandbox/linux/services/linux_syscalls.h"

#if defined(__arm__) && !defined(MAP_STACK)
// Chrome OS Daisy (ARM) build environment has old headers.
#define MAP_STACK 0x20000
#endif

using sandbox::ErrorCode;
using sandbox::SandboxBPF;

namespace nacl {
namespace nonsfi {
namespace {

ErrorCode RestrictFcntlCommands(SandboxBPF* sb) {
  ErrorCode::ArgType mask_long_type;
  if (sizeof(long) == 8) {
    mask_long_type = ErrorCode::TP_64BIT;
  } else if (sizeof(long) == 4) {
    mask_long_type = ErrorCode::TP_32BIT;
  } else {
    NOTREACHED();
  }
  // We allow following cases:
  // 1. F_SETFD + FD_CLOEXEC: libevent's epoll_init uses this.
  // 2. F_GETFL: Used by SetNonBlocking in
  // message_pump_libevent.cc and Channel::ChannelImpl::CreatePipe
  // in ipc_channel_posix.cc. Note that the latter does not work
  // with EPERM.
  // 3. F_SETFL: Used by evutil_make_socket_nonblocking in
  // libevent and SetNonBlocking. As the latter mix O_NONBLOCK to
  // the return value of F_GETFL, so we need to allow O_ACCMODE in
  // addition to O_NONBLOCK.
  const unsigned long denied_mask = ~(O_ACCMODE | O_NONBLOCK);
  return sb->Cond(1, ErrorCode::TP_32BIT,
                  ErrorCode::OP_EQUAL, F_SETFD,
                  sb->Cond(2, mask_long_type,
                           ErrorCode::OP_EQUAL, FD_CLOEXEC,
                           ErrorCode(ErrorCode::ERR_ALLOWED),
                  sb->Trap(sandbox::CrashSIGSYS_Handler, NULL)),
         sb->Cond(1, ErrorCode::TP_32BIT,
                  ErrorCode::OP_EQUAL, F_GETFL,
                  ErrorCode(ErrorCode::ERR_ALLOWED),
         sb->Cond(1, ErrorCode::TP_32BIT,
                  ErrorCode::OP_EQUAL, F_SETFL,
                  sb->Cond(2, mask_long_type,
                           ErrorCode::OP_HAS_ANY_BITS, denied_mask,
                           sb->Trap(sandbox::CrashSIGSYS_Handler, NULL),
                           ErrorCode(ErrorCode::ERR_ALLOWED)),
         sb->Trap(sandbox::CrashSIGSYS_Handler, NULL))));
}

ErrorCode RestrictClockID(SandboxBPF* sb) {
  // We allow accessing only CLOCK_MONOTONIC, CLOCK_PROCESS_CPUTIME_ID,
  // CLOCK_REALTIME, and CLOCK_THREAD_CPUTIME_ID.  In particular, this disallows
  // access to arbitrary per-{process,thread} CPU-time clock IDs (such as those
  // returned by {clock,pthread}_getcpuclockid), which can leak information
  // about the state of the host OS.
  COMPILE_ASSERT(4 == sizeof(clockid_t), clockid_is_not_32bit);
  ErrorCode result = sb->Cond(0, ErrorCode::TP_32BIT,
                              ErrorCode::OP_EQUAL, CLOCK_MONOTONIC,
                              ErrorCode(ErrorCode::ERR_ALLOWED),
                     sb->Cond(0, ErrorCode::TP_32BIT,
                              ErrorCode::OP_EQUAL, CLOCK_PROCESS_CPUTIME_ID,
                              ErrorCode(ErrorCode::ERR_ALLOWED),
                     sb->Cond(0, ErrorCode::TP_32BIT,
                              ErrorCode::OP_EQUAL, CLOCK_REALTIME,
                              ErrorCode(ErrorCode::ERR_ALLOWED),
                     sb->Cond(0, ErrorCode::TP_32BIT,
                              ErrorCode::OP_EQUAL, CLOCK_THREAD_CPUTIME_ID,
                              ErrorCode(ErrorCode::ERR_ALLOWED),
                     sb->Trap(sandbox::CrashSIGSYS_Handler, NULL)))));
#if defined(OS_CHROMEOS)
  // Allow the special clock for Chrome OS used by Chrome tracing.
  result = sb->Cond(0, ErrorCode::TP_32BIT,
                    ErrorCode::OP_EQUAL, base::TimeTicks::kClockSystemTrace,
                    ErrorCode(ErrorCode::ERR_ALLOWED), result);
#endif
  return result;
}

ErrorCode RestrictClone(SandboxBPF* sb) {
  // We allow clone only for new thread creation.
  return sb->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                  CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
                  CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS |
                  CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID,
                  ErrorCode(ErrorCode::ERR_ALLOWED),
         sb->Trap(sandbox::SIGSYSCloneFailure, NULL));
}

ErrorCode RestrictPrctl(SandboxBPF* sb) {
  // base::PlatformThread::SetName() uses PR_SET_NAME so we return
  // EPERM for it. Otherwise, we will raise SIGSYS.
  return sb->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                  PR_SET_NAME, ErrorCode(EPERM),
         sb->Trap(sandbox::SIGSYSPrctlFailure, NULL));
}

#if defined(__i386__)
ErrorCode RestrictSocketcall(SandboxBPF* sb) {
  // We only allow socketpair, sendmsg, and recvmsg.
  return sb->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                  SYS_SOCKETPAIR,
                  ErrorCode(ErrorCode::ERR_ALLOWED),
         sb->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                  SYS_SENDMSG,
                  ErrorCode(ErrorCode::ERR_ALLOWED),
         sb->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                  SYS_RECVMSG,
                  ErrorCode(ErrorCode::ERR_ALLOWED),
         sb->Cond(0, ErrorCode::TP_32BIT, ErrorCode::OP_EQUAL,
                  SYS_SHUTDOWN,
                  ErrorCode(ErrorCode::ERR_ALLOWED),
                  sb->Trap(sandbox::CrashSIGSYS_Handler, NULL)))));
}
#endif

ErrorCode RestrictMprotect(SandboxBPF* sb) {
  // TODO(jln, keescook, drewry): Limit the use of mprotect by adding
  // some features to linux kernel.
  const uint32_t denied_mask = ~(PROT_READ | PROT_WRITE | PROT_EXEC);
  return sb->Cond(2, ErrorCode::TP_32BIT,
                  ErrorCode::OP_HAS_ANY_BITS,
                  denied_mask,
         sb->Trap(sandbox::CrashSIGSYS_Handler, NULL),
                  ErrorCode(ErrorCode::ERR_ALLOWED));
}

ErrorCode RestrictMmap(SandboxBPF* sb) {
  const uint32_t denied_flag_mask = ~(MAP_SHARED | MAP_PRIVATE |
                                      MAP_ANONYMOUS | MAP_STACK | MAP_FIXED);
  // When PROT_EXEC is specified, IRT mmap of Non-SFI NaCl helper
  // calls mmap without PROT_EXEC and then adds PROT_EXEC by mprotect,
  // so we do not need to allow PROT_EXEC in mmap.
  const uint32_t denied_prot_mask = ~(PROT_READ | PROT_WRITE);
  return sb->Cond(3, ErrorCode::TP_32BIT,
                  ErrorCode::OP_HAS_ANY_BITS,
                  denied_flag_mask,
                  sb->Trap(sandbox::CrashSIGSYS_Handler, NULL),
         sb->Cond(2, ErrorCode::TP_32BIT,
                  ErrorCode::OP_HAS_ANY_BITS,
                  denied_prot_mask,
                  sb->Trap(sandbox::CrashSIGSYS_Handler, NULL),
                  ErrorCode(ErrorCode::ERR_ALLOWED)));
}

#if defined(__x86_64__) || defined(__arm__)
ErrorCode RestrictSocketpair(SandboxBPF* sb) {
  // Only allow AF_UNIX, PF_UNIX. Crash if anything else is seen.
  COMPILE_ASSERT(AF_UNIX == PF_UNIX, af_unix_pf_unix_different);
  return sb->Cond(0, ErrorCode::TP_32BIT,
                  ErrorCode::OP_EQUAL, AF_UNIX,
                  ErrorCode(ErrorCode::ERR_ALLOWED),
         sb->Trap(sandbox::CrashSIGSYS_Handler, NULL));
}
#endif

bool IsGracefullyDenied(int sysno) {
  switch (sysno) {
    // libevent tries this first and then falls back to poll if
    // epoll_create fails.
    case __NR_epoll_create:
    // third_party/libevent uses them, but we can just return -1 from
    // them as it is just checking getuid() != geteuid() and
    // getgid() != getegid()
#if defined(__i386__) || defined(__arm__)
    case __NR_getegid32:
    case __NR_geteuid32:
    case __NR_getgid32:
    case __NR_getuid32:
#endif
    case __NR_getegid:
    case __NR_geteuid:
    case __NR_getgid:
    case __NR_getuid:
    // tcmalloc calls madvise in TCMalloc_SystemRelease.
    case __NR_madvise:
    // EPERM instead of SIGSYS as glibc tries to open files in /proc.
    // TODO(hamaji): Remove this when we switch to newlib.
    case __NR_open:
    // For RunSandboxSanityChecks().
    case __NR_ptrace:
    // glibc uses this for its pthread implementation. If we return
    // EPERM for this, glibc will stop using this.
    // TODO(hamaji): newlib does not use this. Make this SIGTRAP once
    // we have switched to newlib.
    case __NR_set_robust_list:
    // This is obsolete in ARM EABI, but x86 glibc indirectly calls
    // this in sysconf.
#if defined(__i386__) || defined(__x86_64__)
    case __NR_time:
#endif
      return true;

    default:
      return false;
  }
}

void RunSandboxSanityChecks() {
  errno = 0;
  // Make a ptrace request with an invalid PID.
  long ptrace_ret = ptrace(PTRACE_PEEKUSER, -1 /* pid */, NULL, NULL);
  CHECK_EQ(-1, ptrace_ret);
  // Without the sandbox on, this ptrace call would ESRCH instead.
  CHECK_EQ(EPERM, errno);
}

}  // namespace

ErrorCode NaClNonSfiBPFSandboxPolicy::EvaluateSyscall(SandboxBPF* sb,
                                                      int sysno) const {
  switch (sysno) {
    // Allowed syscalls.
#if defined(__i386__) || defined(__arm__)
    case __NR__llseek:
#elif defined(__x86_64__)
    case __NR_lseek:
#endif
    case __NR_close:
    case __NR_dup:
    case __NR_dup2:
    case __NR_exit:
    case __NR_exit_group:
#if defined(__i386__) || defined(__arm__)
    case __NR_fstat64:
#elif defined(__x86_64__)
    case __NR_fstat:
#endif
    // TODO(hamaji): Allow only FUTEX_PRIVATE_FLAG.
    case __NR_futex:
    // TODO(hamaji): Remove the need of gettid. Currently, this is
    // called from PlatformThread::CurrentId().
    case __NR_gettid:
    case __NR_gettimeofday:
    case __NR_munmap:
    case __NR_nanosleep:
    // TODO(hamaji): Remove the need of pipe. Currently, this is
    // called from base::MessagePumpLibevent::Init().
    case __NR_pipe:
    case __NR_poll:
    case __NR_pread64:
    case __NR_pwrite64:
    case __NR_read:
    case __NR_restart_syscall:
    case __NR_sched_yield:
    // __NR_times needed as clock() is called by CommandBufferHelper, which is
    // used by NaCl applications that use Pepper's 3D interfaces.
    // See crbug.com/264856 for details.
    case __NR_times:
    case __NR_write:
#if defined(__arm__)
    case __ARM_NR_cacheflush:
#endif
      return ErrorCode(ErrorCode::ERR_ALLOWED);

    case __NR_clock_getres:
    case __NR_clock_gettime:
      return RestrictClockID(sb);

    case __NR_clone:
      return RestrictClone(sb);

#if defined(__x86_64__)
    case __NR_fcntl:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_fcntl64:
#endif
      return RestrictFcntlCommands(sb);

#if defined(__x86_64__)
    case __NR_mmap:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_mmap2:
#endif
      return RestrictMmap(sb);
    case __NR_mprotect:
      return RestrictMprotect(sb);

    case __NR_prctl:
      return RestrictPrctl(sb);

#if defined(__i386__)
    case __NR_socketcall:
      return RestrictSocketcall(sb);
#endif
#if defined(__x86_64__) || defined(__arm__)
    case __NR_recvmsg:
    case __NR_sendmsg:
    case __NR_shutdown:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    case __NR_socketpair:
      return RestrictSocketpair(sb);
#endif

    case __NR_brk:
      // The behavior of brk on Linux is different from other system
      // calls. It does not return errno but the current break on
      // failure. glibc thinks brk failed if the return value of brk
      // is less than the requested address (i.e., brk(addr) < addr).
      // So, glibc thinks brk succeeded if we return -EPERM and we
      // need to return zero instead.
      return ErrorCode(0);

    default:
      if (IsGracefullyDenied(sysno))
        return ErrorCode(EPERM);
      return sb->Trap(sandbox::CrashSIGSYS_Handler, NULL);
  }
}

ErrorCode NaClNonSfiBPFSandboxPolicy::InvalidSyscall(SandboxBPF* sb) const {
  return sb->Trap(sandbox::CrashSIGSYS_Handler, NULL);
}

bool InitializeBPFSandbox() {
  bool sandbox_is_initialized = content::InitializeSandbox(
      scoped_ptr<sandbox::SandboxBPFPolicy>(
          new nacl::nonsfi::NaClNonSfiBPFSandboxPolicy()));
  if (!sandbox_is_initialized)
    return false;
  RunSandboxSanityChecks();
  return true;
}

}  // namespace nonsfi
}  // namespace nacl
