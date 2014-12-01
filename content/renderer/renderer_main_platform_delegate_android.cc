// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"

#ifdef USE_SECCOMP_BPF
#include "content/common/sandbox_linux/android/sandbox_bpf_base_policy_android.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#endif

namespace content {

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters)
    : parameters_(parameters) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::EnableSandbox() {
#ifdef USE_SECCOMP_BPF
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSeccompFilterSandbox)) {
    return true;
  }
  if (!sandbox::SandboxBPF::SupportsSeccompSandbox(
          sandbox::SandboxBPF::SeccompLevel::MULTI_THREADED)) {
    LOG(WARNING) << "Seccomp-BPF sandbox enabled without kernel support. "
                 << "Ignoring flag and proceeding without seccomp sandbox.";
    return true;
  }

  sandbox::SandboxBPF sandbox(new SandboxBPFBasePolicyAndroid());
  CHECK(
      sandbox.StartSandbox(sandbox::SandboxBPF::SeccompLevel::MULTI_THREADED));
#endif
  return true;
}

}  // namespace content
