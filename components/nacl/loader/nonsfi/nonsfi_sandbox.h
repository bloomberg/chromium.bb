// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NONSFI_NONSFI_SANDBOX_H_
#define COMPONENTS_NACL_LOADER_NONSFI_NONSFI_SANDBOX_H_

#include "base/basictypes.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"

namespace nacl {
namespace nonsfi {

// The seccomp sandbox policy for NaCl non-SFI mode. Note that this
// policy must be as strong as possible, as non-SFI mode heavily
// depends on seccomp sandbox.
class NaClNonSfiBPFSandboxPolicy : public sandbox::SandboxBPFPolicy {
 public:
  explicit NaClNonSfiBPFSandboxPolicy() {}
  virtual ~NaClNonSfiBPFSandboxPolicy() {}

  virtual sandbox::ErrorCode EvaluateSyscall(sandbox::SandboxBPF* sb,
                                             int sysno) const OVERRIDE;
  virtual sandbox::ErrorCode InvalidSyscall(
      sandbox::SandboxBPF* sb) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(NaClNonSfiBPFSandboxPolicy);
};

// Initializes seccomp-bpf sandbox for non-SFI NaCl. Returns false on
// failure.
bool InitializeBPFSandbox();

}  // namespace nonsfi
}  // namespace nacl

#endif  // COMPONENTS_NACL_LOADER_NONSFI_NONSFI_SANDBOX_H_
