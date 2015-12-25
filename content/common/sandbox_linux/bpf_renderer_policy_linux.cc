// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/bpf_renderer_policy_linux.h"

#include <errno.h>
#include <sys/ioctl.h>

#include "build/build_config.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

#if defined(USE_VGEM_MAP)
#include <libdrm/vgem_drm.h>
#endif

using sandbox::SyscallSets;
using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::ResultExpr;

namespace content {

namespace {

ResultExpr RestrictIoctl() {
  const Arg<unsigned long> request(1);
  return Switch(request)
      .SANDBOX_BPF_DSL_CASES((static_cast<unsigned long>(TCGETS), FIONREAD),
                             Allow())
#if defined(USE_VGEM_MAP)
      // Type of DRM_IOCTL_XXX is unsigned long on IA and unsigned int on ARM.
      .SANDBOX_BPF_DSL_CASES(
          (static_cast<unsigned long>(DRM_IOCTL_GEM_CLOSE),
           DRM_IOCTL_VGEM_MODE_MAP_DUMB, DRM_IOCTL_PRIME_FD_TO_HANDLE),
          Allow())
#endif
      .Default(sandbox::CrashSIGSYSIoctl());
}

}  // namespace

RendererProcessPolicy::RendererProcessPolicy() {}
RendererProcessPolicy::~RendererProcessPolicy() {}

ResultExpr RendererProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
    // The baseline policy allows __NR_clock_gettime. Allow
    // clock_getres() for V8. crbug.com/329053.
    case __NR_clock_getres:
      return sandbox::RestrictClockID();
    case __NR_ioctl:
      return RestrictIoctl();
    // Allow the system calls below.
    case __NR_fdatasync:
    case __NR_fsync:
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__)
    case __NR_getrlimit:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_ugetrlimit:
#endif
    case __NR_mremap:  // See crbug.com/149834.
    case __NR_pread64:
    case __NR_pwrite64:
    case __NR_sched_get_priority_max:
    case __NR_sched_get_priority_min:
    case __NR_sysinfo:
    case __NR_times:
    case __NR_uname:
      return Allow();
    case __NR_sched_getaffinity:
    case __NR_sched_getparam:
    case __NR_sched_getscheduler:
    case __NR_sched_setscheduler:
      return sandbox::RestrictSchedTarget(GetPolicyPid(), sysno);
    case __NR_prlimit64:
      return Error(EPERM);  // See crbug.com/160157.
    default:
      // Default on the content baseline policy.
      return SandboxBPFBasePolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace content
