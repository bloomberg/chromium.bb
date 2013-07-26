// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/zygote_host_linux.h"
#include "content/public/common/sandbox_linux.h"

typedef InProcessBrowserTest SandboxLinuxTest;

// Both the SUID sandbox (http://crbug.com/137653) and the Seccomp-BPF sandbox
// are currently incompatible with ASan.
#if defined(OS_LINUX) && !defined(ADDRESS_SANITIZER)
#define MAYBE_SandboxStatus \
        SandboxStatus
#else
#define MAYBE_SandboxStatus \
        DISABLED_SandboxStatus
#endif

IN_PROC_BROWSER_TEST_F(SandboxLinuxTest, MAYBE_SandboxStatus) {
  // Get expected sandboxing status of renderers.
  const int status = content::ZygoteHost::GetInstance()->GetSandboxStatus();

  // The setuid sandbox is required as our first-layer sandbox.
  bool good_layer1 = status & content::kSandboxLinuxSUID &&
                     status & content::kSandboxLinuxPIDNS &&
                     status & content::kSandboxLinuxNetNS;
  // A second-layer sandbox is also required to be adequately sandboxed.
  bool good_layer2 = status & content::kSandboxLinuxSeccompBpf;

  EXPECT_TRUE(good_layer1);
  EXPECT_TRUE(good_layer2);
}
