// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_LINUX_BPF_CROS_ARM_GPU_POLICY_LINUX_H_
#define CONTENT_COMMON_SANDBOX_LINUX_BPF_CROS_ARM_GPU_POLICY_LINUX_H_

#include "base/macros.h"
#include "content/common/sandbox_linux/bpf_gpu_policy_linux.h"

namespace content {

// This policy is for Chrome OS ARM.
class CrosArmGpuProcessPolicy : public GpuProcessPolicy {
 public:
  explicit CrosArmGpuProcessPolicy(bool allow_shmat);
  ~CrosArmGpuProcessPolicy() override;

  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(
      int system_call_number) const override;
  bool PreSandboxHook() override;

 private:
#if defined(__arm__) || defined(__aarch64__)
  const bool allow_shmat_;  // Allow shmat(2).
#endif
  DISALLOW_COPY_AND_ASSIGN(CrosArmGpuProcessPolicy);
};

class CrosArmGpuBrokerProcessPolicy : public CrosArmGpuProcessPolicy {
 public:
  CrosArmGpuBrokerProcessPolicy();
  ~CrosArmGpuBrokerProcessPolicy() override;

  sandbox::bpf_dsl::ResultExpr EvaluateSyscall(
      int system_call_number) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosArmGpuBrokerProcessPolicy);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_LINUX_BPF_CROS_ARM_GPU_POLICY_LINUX_H_
