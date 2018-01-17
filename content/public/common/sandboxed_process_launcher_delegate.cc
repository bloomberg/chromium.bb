// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandboxed_process_launcher_delegate.h"

#include "build/build_config.h"
#include "content/public/common/zygote_features.h"

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "content/public/common/zygote_handle.h"
#endif

namespace content {

#if defined(OS_WIN)
bool SandboxedProcessLauncherDelegate::DisableDefaultPolicy() {
  return false;
}

bool SandboxedProcessLauncherDelegate::PreSpawnTarget(
    sandbox::TargetPolicy* policy) {
  return true;
}

void SandboxedProcessLauncherDelegate::PostSpawnTarget(
    base::ProcessHandle process) {}

bool SandboxedProcessLauncherDelegate::ShouldLaunchElevated() {
  return false;
}
#endif  // defined(OS_WIN)

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
ZygoteHandle SandboxedProcessLauncherDelegate::GetZygote() {
  return nullptr;
}
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

#if defined(OS_POSIX)
base::EnvironmentMap SandboxedProcessLauncherDelegate::GetEnvironment() {
  return base::EnvironmentMap();
}
#endif  // defined(OS_POSIX)

}  // namespace content
