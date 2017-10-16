// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_LINUX_BPF_CROS_AMD_GPU_POLICY_LINUX_H_
#define CONTENT_COMMON_SANDBOX_LINUX_BPF_CROS_AMD_GPU_POLICY_LINUX_H_

#include "base/macros.h"
#include "content/common/sandbox_linux/bpf_gpu_policy_linux.h"

namespace content {

// This policy is for AMD GPUs running on Chrome OS.
class CrosAmdGpuProcessPolicy : public GpuProcessPolicy {
 public:
  CrosAmdGpuProcessPolicy();
  ~CrosAmdGpuProcessPolicy() override;

  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(
      int system_call_number) const override;
  bool PreSandboxHook() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosAmdGpuProcessPolicy);
};

class CrosAmdGpuBrokerProcessPolicy : public CrosAmdGpuProcessPolicy {
 public:
  CrosAmdGpuBrokerProcessPolicy();
  ~CrosAmdGpuBrokerProcessPolicy() override;

  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(
      int system_call_number) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosAmdGpuBrokerProcessPolicy);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_LINUX_BPF_CROS_AMD_GPU_POLICY_LINUX_H_
