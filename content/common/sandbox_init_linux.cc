// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandbox_init.h"

#include "base/callback.h"
#include "content/common/sandbox_seccomp_bpf_linux.h"

namespace content {

bool InitializeSandbox(playground2::BpfSandboxPolicy policy) {
  return SandboxSeccompBpf::StartSandboxWithExternalPolicy(policy);
}

playground2::BpfSandboxPolicyCallback GetBpfSandboxBaselinePolicy() {
  return SandboxSeccompBpf::GetBaselinePolicy();
}

}  // namespace content
