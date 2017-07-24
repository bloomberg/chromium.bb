// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_
#define CONTENT_PUBLIC_COMMON_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_

#include <cstddef>

#include "base/environment.h"
#include "base/files/scoped_file.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/sandbox_type.h"
#include "content/public/common/zygote_handle.h"

namespace sandbox {
class TargetPolicy;
}

namespace content {

// Allows a caller of StartSandboxedProcess or
// BrowserChildProcessHost/ChildProcessLauncher to control the sandbox policy,
// i.e. to loosen it if needed.
// The methods below will be called on the PROCESS_LAUNCHER thread.
class CONTENT_EXPORT SandboxedProcessLauncherDelegate {
 public:
  virtual ~SandboxedProcessLauncherDelegate() {}

#if defined(OS_WIN)
  // Override to return true if the process should be launched as an elevated
  // process (which implies no sandbox).
  virtual bool ShouldLaunchElevated();

  // Whether to disable the default policy specified in
  // AddPolicyForSandboxedProcess.
  virtual bool DisableDefaultPolicy();

  // Called right before spawning the process. Returns false on failure.
  virtual bool PreSpawnTarget(sandbox::TargetPolicy* policy);

  // Called right after the process is launched, but before its thread is run.
  virtual void PostSpawnTarget(base::ProcessHandle process) {}

#elif defined(OS_POSIX)

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  // Returns the zygote used to launch the process.
  // NOTE: For now Chrome always uses the same zygote for performance reasons.
  // http://crbug.com/569191
  virtual ZygoteHandle GetZygote();
#endif  // !defined(OS_MACOSX) && !defined(OS_ANDROID)

  // Override this if the process needs a non-empty environment map.
  virtual base::EnvironmentMap GetEnvironment();
#endif

  // Returns the SandboxType to enforce on the process, or
  // SANDBOX_TYPE_NO_SANDBOX to run without a sandbox policy.
  virtual SandboxType GetSandboxType() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SANDBOXED_PROCESS_LAUNCHER_DELEGATE_H_
