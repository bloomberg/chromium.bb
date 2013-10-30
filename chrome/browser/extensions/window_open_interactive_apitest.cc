// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

class WindowOpenPanelTest : public ExtensionApiTest {
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePanels);
  }
};

// http://crbug.com/253417 for NDEBUG
#if defined(OS_WIN) || (defined(OS_MACOSX) && defined(NDEBUG))
// Focus test fails if there is no window manager on Linux.
IN_PROC_BROWSER_TEST_F(WindowOpenPanelTest, WindowOpenFocus) {
  ASSERT_TRUE(RunExtensionTest("window_open/focus")) << message_;
}
#endif
