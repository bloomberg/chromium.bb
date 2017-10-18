// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/bpf_cros_amd_gpu_policy_linux.h"

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

#include "base/logging.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;

namespace content {

CrosAmdGpuProcessPolicy::CrosAmdGpuProcessPolicy() {}

CrosAmdGpuProcessPolicy::~CrosAmdGpuProcessPolicy() {}

ResultExpr CrosAmdGpuProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
    case __NR_fstatfs:
    case __NR_sched_setscheduler:
    case __NR_sysinfo:
    case __NR_uname:
#if !defined(__aarch64__)
    case __NR_getdents:
    case __NR_readlink:
    case __NR_stat:
#endif
      return Allow();
#if defined(__x86_64__)
    // Allow only AF_UNIX for |domain|.
    case __NR_socket:
    case __NR_socketpair: {
      const Arg<int> domain(0);
      return If(domain == AF_UNIX, Allow()).Else(Error(EPERM));
    }
#endif
    default:
      // Default to the generic GPU policy.
      return GpuProcessPolicy::EvaluateSyscall(sysno);
  }
}

CrosAmdGpuBrokerProcessPolicy ::CrosAmdGpuBrokerProcessPolicy() {}

CrosAmdGpuBrokerProcessPolicy ::~CrosAmdGpuBrokerProcessPolicy() {}

// A GPU broker policy is the same as a GPU policy with access, open,
// openat and in the non-Chrome OS case unlink allowed.
ResultExpr CrosAmdGpuBrokerProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
    case __NR_faccessat:
    case __NR_openat:
#if !defined(__aarch64__)
    case __NR_access:
    case __NR_open:
#if !defined(OS_CHROMEOS)
    // The broker process needs to able to unlink the temporary
    // files that it may create. This is used by DRI3.
    case __NR_unlink:
#endif  // !defined(OS_CHROMEOS)
#endif  // !define(__aarch64__)
      return Allow();
    default:
      return CrosAmdGpuProcessPolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace content
