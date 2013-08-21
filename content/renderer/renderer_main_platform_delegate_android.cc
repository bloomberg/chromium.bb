// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"
#include "base/logging.h"

#ifdef ENABLE_VTUNE_JIT_INTERFACE
#include "content/public/common/content_switches.h"
#include "v8/src/third_party/vtune/v8-vtune.h"
#endif

namespace content {

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters)
    : parameters_(parameters) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
#ifdef ENABLE_VTUNE_JIT_INTERFACE
  const CommandLine& command_line = parameters_.command_line;
  if (command_line.HasSwitch(switches::kEnableVtune))
    vTune::InitializeVtuneForV8();
#endif
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  return true;
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  return true;
}

void RendererMainPlatformDelegate::RunSandboxTests(bool no_sandbox) {
}

}  // namespace content
