// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#include <errno.h>
#include <sys/stat.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "content/common/sandbox_linux.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"

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

bool RendererMainPlatformDelegate::InitSandboxTests(bool no_sandbox) {
  // The sandbox is started in the zygote process: zygote_main_linux.cc
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox
  return true;
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  // The setuid sandbox is started in the zygote process: zygote_main_linux.cc
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandbox
  //
  // Anything else is started in InitializeSandbox().
  LinuxSandbox::InitializeSandbox();
  return true;
}

void RendererMainPlatformDelegate::RunSandboxTests(bool no_sandbox) {
  // The LinuxSandbox class requires going through initialization before
  // GetStatus() and others can be used.  When we are not launched through the
  // Zygote, this initialization will only happen in the renderer process if
  // EnableSandbox() above is called, which it won't necesserily be.
  // This only happens with flags such as --renderer-cmd-prefix which are
  // for debugging.
  if (no_sandbox)
    return;

  // about:sandbox uses a value returned from LinuxSandbox::GetStatus() before
  // any renderer has been started.
  // Here, we test that the status of SeccompBpf in the renderer is consistent
  // with what LinuxSandbox::GetStatus() said we would do.
  class LinuxSandbox* linux_sandbox = LinuxSandbox::GetInstance();
  if (linux_sandbox->GetStatus() & kSandboxLinuxSeccompBpf) {
    CHECK(linux_sandbox->seccomp_bpf_started());
  }

  // Under the setuid sandbox, we should not be able to open any file via the
  // filesystem.
  if (linux_sandbox->GetStatus() & kSandboxLinuxSUID) {
    CHECK(!file_util::PathExists(base::FilePath("/proc/cpuinfo")));
  }

#if defined(__x86_64__)
  // Limit this test to architectures where seccomp BPF is active in renderers.
  if (linux_sandbox->seccomp_bpf_started()) {
    errno = 0;
    // This should normally return EBADF since the first argument is bogus,
    // but we know that under the seccomp-bpf sandbox, this should return EPERM.
    CHECK_EQ(fchmod(-1, 07777), -1);
    CHECK_EQ(errno, EPERM);
  }
#endif  // __x86_64__
}

}  // namespace content
