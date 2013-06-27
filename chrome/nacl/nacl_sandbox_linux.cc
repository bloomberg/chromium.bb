// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/nacl/nacl_sandbox_linux.h"

#include <signal.h>
#include <sys/ptrace.h>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/public/common/sandbox_init.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/linux_syscalls.h"

using playground2::ErrorCode;
using playground2::Sandbox;

namespace {

// This policy does very little:
// - Any invalid system call for the current architecture is handled by
//   the baseline policy.
// - ptrace() is denied.
// - Anything else is allowed.
// Note that the seccomp-bpf sandbox always prevents cross-architecture
// system calls (on x86, long/compatibility/x32).
// So even this trivial policy has a security benefit.
ErrorCode NaClBpfSandboxPolicy(
    playground2::Sandbox* sb, int sysnum, void* aux) {
  const playground2::BpfSandboxPolicyCallback baseline_policy =
      content::GetBpfSandboxBaselinePolicy();
  if (!playground2::Sandbox::IsValidSyscallNumber(sysnum)) {
    return baseline_policy.Run(sb, sysnum, aux);
  }
  switch (sysnum) {
    case __NR_ptrace:
      return ErrorCode(EPERM);
    default:
      return ErrorCode(ErrorCode::ERR_ALLOWED);
  }
  NOTREACHED();
  // GCC wants this.
  return ErrorCode(EPERM);
}

void RunSandboxSanityChecks() {
  errno = 0;
  // Make a ptrace request with an invalid PID.
  long ptrace_ret = ptrace(PTRACE_PEEKUSER, -1 /* pid */, NULL, NULL);
  CHECK_EQ(-1, ptrace_ret);
  // Without the sandbox on, this ptrace call would ESRCH instead.
  CHECK_EQ(EPERM, errno);
}

}  // namespace

bool InitializeBpfSandbox() {
  bool sandbox_is_initialized =
      content::InitializeSandbox(NaClBpfSandboxPolicy);
  if (sandbox_is_initialized) {
    RunSandboxSanityChecks();
    // TODO(jln): Find a way to fix this.
    // The sandbox' SIGSYS handler trips NaCl, so we disable it.
    // If SIGSYS is triggered it'll now execute the default action
    // (CORE). This will make it hard to track down bugs and sandbox violations.
    CHECK(signal(SIGSYS, SIG_DFL) != SIG_ERR);
    return true;
  }
  return false;
}
