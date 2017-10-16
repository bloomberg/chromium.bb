// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandbox_init.h"

#include <memory>
#include <utility>

#include "base/files/scoped_file.h"
#include "build/build_config.h"
#include "content/common/sandbox_linux/sandbox_seccomp_bpf_linux.h"
#include "sandbox/linux/bpf_dsl/policy.h"

namespace content {

bool InitializeSandbox(std::unique_ptr<sandbox::bpf_dsl::Policy> policy,
                       base::ScopedFD proc_fd) {
  return SandboxSeccompBPF::StartSandboxWithExternalPolicy(std::move(policy),
                                                           std::move(proc_fd));
}

#if !defined(OS_NACL_NONSFI)
std::unique_ptr<sandbox::bpf_dsl::Policy> GetBPFSandboxBaselinePolicy() {
  return SandboxSeccompBPF::GetBaselinePolicy();
}
#endif  // !defined(OS_NACL_NONSFI)

}  // namespace content
