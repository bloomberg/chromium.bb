// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

// In the touch build, this fails reliably. http://crbug.com/85192
#if defined(TOUCH_UI)
#define MAYBE_Sidebar DISABLED_Sidebar
#else
#define MAYBE_Sidebar Sidebar
#endif

class SidebarApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

IN_PROC_BROWSER_TEST_F(SidebarApiTest, MAYBE_Sidebar) {
  ASSERT_TRUE(RunExtensionTest("sidebar")) << message_;
}
