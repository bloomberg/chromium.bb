// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_LINUX_SANDBOX_BPF_BASE_POLICY_LINUX_H_
#define CONTENT_COMMON_SANDBOX_LINUX_SANDBOX_BPF_BASE_POLICY_LINUX_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"

namespace content {

// The "baseline" BPF policy for content/. Any content/ seccomp-bpf policy
// should inherit from it.
// It implements the main SandboxBPFDSLPolicy interface. Due to its nature
// as a "kernel attack surface reduction" layer, it's implementation-defined.
class SandboxBPFBasePolicy : public sandbox::bpf_dsl::SandboxBPFDSLPolicy {
 public:
  SandboxBPFBasePolicy();
  virtual ~SandboxBPFBasePolicy();

  virtual sandbox::bpf_dsl::ResultExpr EvaluateSyscall(
      int system_call_number) const OVERRIDE;
  virtual sandbox::bpf_dsl::ResultExpr InvalidSyscall() const OVERRIDE;

  // A policy can implement this hook to run code right before the policy
  // is passed to the BPF compiler and the sandbox is engaged.
  // If PreSandboxHook() returns true, the sandbox is guaranteed to be
  // engaged afterwards.
  // This will be used when enabling the sandbox though
  // SandboxSeccompBPF::StartSandbox().
  virtual bool PreSandboxHook();

  // Get the errno(3) to return for filesystem errors.
  static int GetFSDeniedErrno();

 private:
  // Compose the BaselinePolicy from sandbox/.
  scoped_ptr<sandbox::BaselinePolicy> baseline_policy_;
  DISALLOW_COPY_AND_ASSIGN(SandboxBPFBasePolicy);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_LINUX_SANDBOX_BPF_BASE_POLICY_LINUX_H_
