// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/nacl/nacl_main_platform_delegate.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "seccompsandbox/sandbox.h"

NaClMainPlatformDelegate::NaClMainPlatformDelegate(
    const MainFunctionParams& parameters)
    : parameters_(parameters), sandbox_test_module_(NULL) {
}

NaClMainPlatformDelegate::~NaClMainPlatformDelegate() {
}

void NaClMainPlatformDelegate::PlatformInitialize() {
}

void NaClMainPlatformDelegate::PlatformUninitialize() {
}

void NaClMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  // The sandbox is started in the zygote process: zygote_main_linux.cc
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox
  return;
}

void NaClMainPlatformDelegate::EnableSandbox() {
  // The setuid sandbox is started in the zygote process: zygote_main_linux.cc
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox
  //
  // The seccomp sandbox is started in the renderer.
  // http://code.google.com/p/seccompsandbox/
#if defined(ARCH_CPU_X86_FAMILY) && !defined(CHROMIUM_SELINUX) && \
    !defined(__clang__)
  // N.b. SupportsSeccompSandbox() returns a cached result, as we already
  // called it earlier in the zygote. Thus, it is OK for us to not pass in
  // a file descriptor for "/proc".
  if (switches::SeccompSandboxEnabled() && SupportsSeccompSandbox(-1))
    StartSeccompSandbox();
#endif
}

bool NaClMainPlatformDelegate::RunSandboxTests() {
  // The sandbox is started in the zygote process: zygote_main_linux.cc
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox
  return true;
}
