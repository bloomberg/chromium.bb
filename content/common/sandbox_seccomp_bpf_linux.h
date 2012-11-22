// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_SECCOMP_BPF_LINUX_H_
#define CONTENT_COMMON_SANDBOX_SECCOMP_BPF_LINUX_H_

#include "base/basictypes.h"

namespace content {

class SandboxSeccompBpf {
 public:
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

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SandboxSeccompBpf);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_SECCOMP_BPF_LINUX_H_

