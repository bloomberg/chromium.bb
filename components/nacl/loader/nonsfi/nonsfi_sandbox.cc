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
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"
#include "sandbox/linux/services/linux_syscalls.h"

#if defined(__arm__) && !defined(MAP_STACK)
// Chrome OS Daisy (ARM) build environment has old headers.
#define MAP_STACK 0x20000
#endif

using namespace sandbox::bpf_dsl;
using sandbox::CrashSIGSYS;
using sandbox::CrashSIGSYSClone;
using sandbox::CrashSIGSYSPrctl;

namespace nacl {
namespace nonsfi {
namespace {

ResultExpr RestrictFcntlCommands() {
  const Arg<int> cmd(1);
  const Arg<long> long_arg(2);

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
  return If((cmd == F_SETFD && long_arg == FD_CLOEXEC) || cmd == F_GETFL ||
                (cmd == F_SETFL && (long_arg & denied_mask) == 0),
            Allow()).Else(CrashSIGSYS());
}

ResultExpr RestrictClockID() {
  // We allow accessing only CLOCK_MONOTONIC, CLOCK_PROCESS_CPUTIME_ID,
  // CLOCK_REALTIME, and CLOCK_THREAD_CPUTIME_ID.  In particular, this disallows
  // access to arbitrary per-{process,thread} CPU-time clock IDs (such as those
  // returned by {clock,pthread}_getcpuclockid), which can leak information
  // about the state of the host OS.
  COMPILE_ASSERT(4 == sizeof(clockid_t), clockid_is_not_32bit);
  const Arg<clockid_t> clockid(0);
  return If(
#if defined(OS_CHROMEOS)
             // Allow the special clock for Chrome OS used by Chrome tracing.
             clockid == base::TimeTicks::kClockSystemTrace ||
#endif
                 clockid == CLOCK_MONOTONIC ||
                 clockid == CLOCK_PROCESS_CPUTIME_ID ||
                 clockid == CLOCK_REALTIME ||
                 clockid == CLOCK_THREAD_CPUTIME_ID,
             Allow()).Else(CrashSIGSYS());
}

ResultExpr RestrictClone() {
  // We allow clone only for new thread creation.
  const Arg<int> flags(0);
  return If(flags == (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
                      CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS |
                      CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID),
            Allow()).Else(CrashSIGSYSClone());
}

ResultExpr RestrictPrctl() {
  // base::PlatformThread::SetName() uses PR_SET_NAME so we return
  // EPERM for it. Otherwise, we will raise SIGSYS.
  const Arg<int> option(0);
  return If(option == PR_SET_NAME, Error(EPERM)).Else(CrashSIGSYSPrctl());
}

#if defined(__i386__)
ResultExpr RestrictSocketcall() {
  // We only allow socketpair, sendmsg, and recvmsg.
  const Arg<int> call(0);
  return If(call == SYS_SOCKETPAIR || call == SYS_SHUTDOWN ||
                call == SYS_SENDMSG || call == SYS_RECVMSG,
            Allow()).Else(CrashSIGSYS());
}
#endif

ResultExpr RestrictMprotect() {
  // TODO(jln, keescook, drewry): Limit the use of mprotect by adding
  // some features to linux kernel.
  const uint32_t denied_mask = ~(PROT_READ | PROT_WRITE | PROT_EXEC);
  const Arg<int> prot(2);
  return If((prot & denied_mask) == 0, Allow()).Else(CrashSIGSYS());
}

ResultExpr RestrictMmap() {
  const uint32_t denied_flag_mask = ~(MAP_SHARED | MAP_PRIVATE |
                                      MAP_ANONYMOUS | MAP_STACK | MAP_FIXED);
  // When PROT_EXEC is specified, IRT mmap of Non-SFI NaCl helper
  // calls mmap without PROT_EXEC and then adds PROT_EXEC by mprotect,
  // so we do not need to allow PROT_EXEC in mmap.
  const uint32_t denied_prot_mask = ~(PROT_READ | PROT_WRITE);
  const Arg<int> prot(2), flags(3);
  return If((prot & denied_prot_mask) == 0 && (flags & denied_flag_mask) == 0,
            Allow()).Else(CrashSIGSYS());
}

#if defined(__x86_64__) || defined(__arm__)
ResultExpr RestrictSocketpair() {
  // Only allow AF_UNIX, PF_UNIX. Crash if anything else is seen.
  COMPILE_ASSERT(AF_UNIX == PF_UNIX, af_unix_pf_unix_different);
  const Arg<int> domain(0);
  return If(domain == AF_UNIX, Allow()).Else(CrashSIGSYS());
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

ResultExpr NaClNonSfiBPFSandboxPolicy::EvaluateSyscall(int sysno) const {
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
      return Allow();

    case __NR_clock_getres:
    case __NR_clock_gettime:
      return RestrictClockID();

    case __NR_clone:
      return RestrictClone();

#if defined(__x86_64__)
    case __NR_fcntl:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_fcntl64:
#endif
      return RestrictFcntlCommands();

#if defined(__x86_64__)
    case __NR_mmap:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_mmap2:
#endif
      return RestrictMmap();
    case __NR_mprotect:
      return RestrictMprotect();

    case __NR_prctl:
      return RestrictPrctl();

#if defined(__i386__)
    case __NR_socketcall:
      return RestrictSocketcall();
#endif
#if defined(__x86_64__) || defined(__arm__)
    case __NR_recvmsg:
    case __NR_sendmsg:
    case __NR_shutdown:
      return Allow();
    case __NR_socketpair:
      return RestrictSocketpair();
#endif

    case __NR_brk:
      // The behavior of brk on Linux is different from other system
      // calls. It does not return errno but the current break on
      // failure. glibc thinks brk failed if the return value of brk
      // is less than the requested address (i.e., brk(addr) < addr).
      // So, glibc thinks brk succeeded if we return -EPERM and we
      // need to return zero instead.
      return Error(0);

    default:
      if (IsGracefullyDenied(sysno))
        return Error(EPERM);
      return CrashSIGSYS();
  }
}

ResultExpr NaClNonSfiBPFSandboxPolicy::InvalidSyscall() const {
  return CrashSIGSYS();
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
