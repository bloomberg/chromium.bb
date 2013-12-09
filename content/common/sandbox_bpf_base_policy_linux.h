// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_BPF_BASE_POLICY_LINUX_H_
#define CONTENT_COMMON_SANDBOX_BPF_BASE_POLICY_LINUX_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"

using playground2::ErrorCode;
using playground2::Sandbox;

namespace content {

// The "baseline" BPF policy for content/. Any content/ seccomp-bpf policy
// should inherit from it.
// It implements the main SandboxBpfPolicy interface. Due to its nature
// as a "kernel attack surface reduction" layer, it's implementation-defined.
class SandboxBpfBasePolicy : public playground2::SandboxBpfPolicy {
 public:
  SandboxBpfBasePolicy();
  virtual ~SandboxBpfBasePolicy();

  virtual ErrorCode EvaluateSyscall(Sandbox* sandbox_compiler,
                                    int system_call_number) const OVERRIDE;
  // Get the errno(3) to return for filesystem errors.
  static int GetFSDeniedErrno();

 private:
  // Compose the BaselinePolicy from sandbox/.
  scoped_ptr<sandbox::BaselinePolicy> baseline_policy_;
  DISALLOW_COPY_AND_ASSIGN(SandboxBpfBasePolicy);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_BPF_BASE_POLICY_LINUX_H_
