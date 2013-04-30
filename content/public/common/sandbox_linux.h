// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SANDBOX_LINUX_H_
#define CONTENT_PUBLIC_COMMON_SANDBOX_LINUX_H_

namespace content {

// These form a bitmask which describes the conditions of the Linux sandbox.
// Note: this doesn't strictly give you the current status, it states
// what will be enabled when the relevant processes are initialized.
enum LinuxSandboxStatus {
  // SUID sandbox active.
  kSandboxLinuxSUID = 1 << 0,

  // SUID sandbox is using the PID namespace.
  kSandboxLinuxPIDNS = 1 << 1,

  // SUID sandbox is using the network namespace.
  kSandboxLinuxNetNS = 1 << 2,

  // seccomp-bpf sandbox active.
  kSandboxLinuxSeccompBpf = 1 << 3,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SANDBOX_LINUX_H_
