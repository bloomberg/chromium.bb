// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/android/sandbox_bpf_base_policy_android.h"

#include <sys/types.h>

#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::ResultExpr;

namespace content {

SandboxBPFBasePolicyAndroid::SandboxBPFBasePolicyAndroid()
    : SandboxBPFBasePolicy() {}

SandboxBPFBasePolicyAndroid::~SandboxBPFBasePolicyAndroid() {}

ResultExpr SandboxBPFBasePolicyAndroid::EvaluateSyscall(int sysno) const {
  bool override_and_allow = false;

  switch (sysno) {
    // TODO(rsesek): restrict clone parameters.
    case __NR_clone:
    case __NR_epoll_pwait:
    case __NR_flock:
    case __NR_getpriority:
    case __NR_ioctl:
    case __NR_mremap:
    // File system access cannot be restricted with seccomp-bpf on Android,
    // since the JVM classloader and other Framework features require file
    // access. It may be possible to restrict the filesystem with SELinux.
    // Currently we rely on the app/service UID isolation to create a
    // filesystem "sandbox".
#if !ARCH_CPU_ARM64
    case __NR_open:
#endif
    case __NR_openat:
    case __NR_pread64:
    case __NR_rt_sigtimedwait:
    case __NR_setpriority:
    case __NR_sigaltstack:
#if defined(__i386__) || defined(__arm__)
    case __NR_ugetrlimit:
#else
    case __NR_getrlimit:
#endif
    case __NR_uname:
      override_and_allow = true;
      break;
  }

  if (override_and_allow)
    return Allow();

  return SandboxBPFBasePolicy::EvaluateSyscall(sysno);
}

}  // namespace content
