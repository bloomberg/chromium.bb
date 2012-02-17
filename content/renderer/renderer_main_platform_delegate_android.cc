// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"
#include "base/logging.h"

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const content::MainFunctionParams& parameters)
    : parameters_(parameters) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
  NOTIMPLEMENTED();
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
  NOTIMPLEMENTED();
}

bool RendererMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  NOTIMPLEMENTED();
  return true;
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  NOTIMPLEMENTED();
  return true;
}

void RendererMainPlatformDelegate::RunSandboxTests() {
  NOTIMPLEMENTED();
}
