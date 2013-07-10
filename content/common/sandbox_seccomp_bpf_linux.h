// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_SECCOMP_BPF_LINUX_H_
#define CONTENT_COMMON_SANDBOX_SECCOMP_BPF_LINUX_H_

#include <string>

#include "base/basictypes.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy_forward.h"

namespace content {

// This class has two main sets of APIs. One can be used to start the sandbox
// for internal content process types, the other is indirectly exposed as
// a public content/ API and uses a supplied policy.
class SandboxSeccompBpf {
 public:
  // This is the API to enable a seccomp-bpf sandbox for content/
  // process-types:
  // Is the sandbox globally enabled, can anything use it at all ?
  // This looks at global command line flags to see if the sandbox
  // should be enabled at all.
  static bool IsSeccompBpfDesired();
  // Should the sandbox be enabled for process_type ?
  static bool ShouldEnableSeccompBpf(const std::string& process_type);
  // Check if the kernel supports this sandbox. It's useful to "prewarm"
  // this, part of the result will be cached.
  static bool SupportsSandbox();
  // Start the sandbox and apply the policy for process_type, depending on
  // command line switches.
  static bool StartSandbox(const std::string& process_type);

  // This is the API to enable a seccomp-bpf sandbox by using an
  // external policy.
  static bool StartSandboxWithExternalPolicy(
      playground2::BpfSandboxPolicy policy);
  // The "baseline" policy can be a useful base to build a sandbox policy.
  static playground2::BpfSandboxPolicyCallback GetBaselinePolicy();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SandboxSeccompBpf);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_SECCOMP_BPF_LINUX_H_

