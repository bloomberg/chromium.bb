// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nacl_main_platform_delegate.h"

NaClMainPlatformDelegate::NaClMainPlatformDelegate(
    const content::MainFunctionParams& parameters)
    : parameters_(parameters) {
}

NaClMainPlatformDelegate::~NaClMainPlatformDelegate() {
}

void NaClMainPlatformDelegate::EnableSandbox() {
  // The setuid sandbox is started in the zygote process: zygote_main_linux.cc
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox
  //
  // The seccomp sandbox is started in the renderer.
  // http://code.google.com/p/seccompsandbox/
  // seccomp is currently disabled for nacl.
  // http://code.google.com/p/chromium/issues/detail?id=59423
  // See the code in chrome/renderer/renderer_main_platform_delegate_linux.cc
  // for how to turn seccomp on.
  //
  // The seccomp sandbox should not be enabled for Native Client until
  // all of these issues are fixed:
  // http://code.google.com/p/nativeclient/issues/list?q=label:Seccomp
  // At best, NaCl will not work.  At worst, enabling the seccomp sandbox
  // could create a hole in the NaCl sandbox.
}
