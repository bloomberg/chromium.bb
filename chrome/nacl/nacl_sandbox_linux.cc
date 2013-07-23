// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/nacl/nacl_sandbox_linux.h"

#include <signal.h>
#include <sys/ptrace.h>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/common/sandbox_init.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/linux_syscalls.h"

using playground2::ErrorCode;
using playground2::Sandbox;

namespace {

inline bool IsPlatformX86() {
#if defined(__x86_64__) || defined(__i386__)
  return true;
#else
  return false;
#endif
}

// On ARM and x86_64, System V shared memory calls have each their own system
// call, while on i386 they are multiplexed.
#if defined(__x86_64__) || defined(__arm__)
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

ErrorCode NaClBpfSandboxPolicy(
    playground2::Sandbox* sb, int sysno, void* aux) {
  const playground2::BpfSandboxPolicyCallback baseline_policy =
      content::GetBpfSandboxBaselinePolicy();
  switch (sysno) {
    // TODO(jln): NaCl's GDB debug stub uses the following socket system calls,
    // see if it can be restricted a bit.
#if defined(__x86_64__) || defined(__arm__)
    // transport_common.cc needs this.
    case __NR_accept:
    case __NR_setsockopt:
#elif defined(__i386__)
    case __NR_socketcall:
#endif
    // trusted/service_runtime/linux/thread_suspension.c needs sigwait() and is
    // used by NaCl's GDB debug stub.
    case __NR_rt_sigtimedwait:
#if defined(__i386__)
    // Needed on i386 to set-up the custom segments.
    case __NR_modify_ldt:
#endif
    // NaClAddrSpaceBeforeAlloc needs prlimit64.
    case __NR_prlimit64:
    // NaCl uses custom signal stacks.
    case __NR_sigaltstack:
    // Below is fairly similar to the policy for a Chromium renderer.
    // TODO(jln): restrict clone(), ioctl() and prctl().
    case __NR_ioctl:
#if defined(__i386__) || defined(__x86_64__)
    case __NR_getrlimit:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_ugetrlimit:
#endif
    case __NR_pread64:
    case __NR_pwrite64:
    case __NR_sched_get_priority_max:
    case __NR_sched_get_priority_min:
    case __NR_sched_getaffinity:
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_setscheduler:
    case __NR_setpriority:
    case __NR_sysinfo:
    case __NR_uname:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
    case __NR_ptrace:
      return ErrorCode(EPERM);
    default:
      // TODO(jln): look into getting rid of System V shared memory:
      // platform_qualify/linux/sysv_shm_and_mmap.c makes it a requirement, but
      // it may not be needed in all cases. Chromium renderers don't need
      // System V shared memory on Aura.
#if defined(__x86_64__) || defined(__arm__)
      if (IsSystemVSharedMemory(sysno))
        return ErrorCode(ErrorCode::ERR_ALLOWED);
#elif defined(__i386__)
      if (IsSystemVIpc(sysno))
        return ErrorCode(ErrorCode::ERR_ALLOWED);
#endif
      return baseline_policy.Run(sb, sysno, aux);
  }
  NOTREACHED();
  // GCC wants this.
  return ErrorCode(EPERM);
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

bool InitializeBpfSandbox() {
  // TODO(jln): enable the sandbox on ARM as well.
  if (!IsPlatformX86())
    return false;
  bool sandbox_is_initialized =
      content::InitializeSandbox(NaClBpfSandboxPolicy);
  if (sandbox_is_initialized) {
    RunSandboxSanityChecks();
    // TODO(jln): Find a way to fix this.
    // The sandbox' SIGSYS handler trips NaCl, so we disable it.
    // If SIGSYS is triggered it'll now execute the default action
    // (CORE). This will make it hard to track down bugs and sandbox violations.
    CHECK(signal(SIGSYS, SIG_DFL) != SIG_ERR);
    return true;
  }
  return false;
}
