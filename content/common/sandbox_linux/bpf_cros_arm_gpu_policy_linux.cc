// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/bpf_cros_arm_gpu_policy_linux.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/common/sandbox_init_gpu_linux.h"
#include "content/common/sandbox_linux/sandbox_bpf_base_policy_linux.h"
#include "content/common/sandbox_linux/sandbox_seccomp_bpf_linux.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::SyscallSets;

namespace content {

CrosArmGpuProcessPolicy::CrosArmGpuProcessPolicy(bool allow_shmat)
#if defined(__arm__) || defined(__aarch64__)
    : allow_shmat_(allow_shmat)
#endif
{
}

CrosArmGpuProcessPolicy::~CrosArmGpuProcessPolicy() {}

ResultExpr CrosArmGpuProcessPolicy::EvaluateSyscall(int sysno) const {
#if defined(__arm__) || defined(__aarch64__)
  if (allow_shmat_ && sysno == __NR_shmat)
    return Allow();
#endif  // defined(__arm__) || defined(__aarch64__)

  switch (sysno) {
#if defined(__arm__) || defined(__aarch64__)
    // ARM GPU sandbox is started earlier so we need to allow networking
    // in the sandbox.
    case __NR_connect:
    case __NR_getpeername:
    case __NR_getsockname:
    case __NR_sysinfo:
    case __NR_uname:
      return Allow();
    // Allow only AF_UNIX for |domain|.
    case __NR_socket:
    case __NR_socketpair: {
      const Arg<int> domain(0);
      return If(domain == AF_UNIX, Allow()).Else(Error(EPERM));
    }
#endif  // defined(__arm__) || defined(__aarch64__)
    default:
      // Default to the generic GPU policy.
      return GpuProcessPolicy::EvaluateSyscall(sysno);
  }
}

bool CrosArmGpuProcessPolicy::PreSandboxHook() {
  return CrosArmGpuPreSandboxHook(this);
}

CrosArmGpuBrokerProcessPolicy::CrosArmGpuBrokerProcessPolicy()
    : CrosArmGpuProcessPolicy(false) {}

CrosArmGpuBrokerProcessPolicy::~CrosArmGpuBrokerProcessPolicy() {}

// A GPU broker policy is the same as a GPU policy with open and
// openat allowed.
ResultExpr CrosArmGpuBrokerProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
#if !defined(__aarch64__)
    case __NR_access:
    case __NR_open:
#endif  // !defined(__aarch64__)
    case __NR_faccessat:
    case __NR_openat:
      return Allow();
    default:
      return CrosArmGpuProcessPolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace content
