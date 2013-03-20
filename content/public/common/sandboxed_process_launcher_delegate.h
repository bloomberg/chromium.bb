// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_
#define CONTENT_PUBLIC_COMMON_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_

#include "base/process.h"

namespace base {
class FilePath;
}

namespace sandbox {
class TargetPolicy;
}

namespace content {

// Allows a caller of StartSandboxedProcess to control the sandbox policy, i.e.
// to loosen it if needed.
// The methods below will be called on the PROCESS_LAUNCHER thread.
class SandboxedProcessLauncherDelegate {
 public:
  virtual ~SandboxedProcessLauncherDelegate() {}

  // Called before the default sandbox is applied. If the default policy is too
  // restrictive, the caller should set |disable_default_policy| to true and
  // apply their policy in PreSpawnTarget. |exposed_dir| is used to allow a
  //directory through the sandbox.
  virtual void PreSandbox(bool* disable_default_policy,
                          base::FilePath* exposed_dir) {}

  // Called right before spawning the process.
  virtual void PreSpawnTarget(sandbox::TargetPolicy* policy,
                              bool* success) {}

  // Called right after the process is launched, but before its thread is run.
  virtual void PostSpawnTarget(base::ProcessHandle process) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_
