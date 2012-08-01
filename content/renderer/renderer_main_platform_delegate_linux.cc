// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const content::MainFunctionParams& parameters)
    : parameters_(parameters) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  // The sandbox is started in the zygote process: zygote_main_linux.cc
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox
  return true;
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  // The setuid sandbox is started in the zygote process: zygote_main_linux.cc
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox
  //
  // The seccomp sandbox mode 1 (sandbox/linux/seccomp-legacy) and mode 2
  // (sandbox/linux/seccomp-bpf) are started in InitializeSandbox().
  content::InitializeSandbox();
  return true;
}

void RendererMainPlatformDelegate::RunSandboxTests() {
  // The sandbox is started in the zygote process: zygote_main_linux.cc
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox
}
