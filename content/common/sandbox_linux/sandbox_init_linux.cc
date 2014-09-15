// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandbox_init.h"

#include "base/memory/scoped_ptr.h"
#include "content/common/sandbox_linux/sandbox_seccomp_bpf_linux.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"

namespace content {

bool InitializeSandbox(
    scoped_ptr<sandbox::bpf_dsl::SandboxBPFDSLPolicy> policy) {
  return SandboxSeccompBPF::StartSandboxWithExternalPolicy(policy.Pass());
}

scoped_ptr<sandbox::bpf_dsl::SandboxBPFDSLPolicy>
GetBPFSandboxBaselinePolicy() {
  return SandboxSeccompBPF::GetBaselinePolicy().Pass();
}

}  // namespace content
