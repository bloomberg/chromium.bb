// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#include "base/command_line.h"
#include "content/common/content_switches.h"

// This #ifdef logic must be kept in sync with zygote_main_linux.cc.
// TODO(evan): this file doesn't do anything anyway, we should delete it.
#if defined(ARCH_CPU_X86_FAMILY) && !defined(CHROMIUM_SELINUX) && \
  !defined(__clang__) && !defined(OS_CHROMEOS) && !defined(TOOLKIT_VIEWS)
#define SECCOMP_SANDBOX
#include "seccompsandbox/sandbox.h"
#endif

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

bool RendererMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  // The sandbox is started in the zygote process: zygote_main_linux.cc
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox
  return true;
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  // The setuid sandbox is started in the zygote process: zygote_main_linux.cc
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox
  //
  // The seccomp sandbox is started in the renderer.
  // http://code.google.com/p/seccompsandbox/
#if defined(SECCOMP_SANDBOX)
  // N.b. SupportsSeccompSandbox() returns a cached result, as we already
  // called it earlier in the zygote. Thus, it is OK for us to not pass in
  // a file descriptor for "/proc".
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSeccompSandbox) && SupportsSeccompSandbox(-1))
    StartSeccompSandbox();
#endif
  return true;
}

void RendererMainPlatformDelegate::RunSandboxTests() {
  // The sandbox is started in the zygote process: zygote_main_linux.cc
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox
}
