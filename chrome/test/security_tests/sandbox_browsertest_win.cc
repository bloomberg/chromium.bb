// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"

class SandboxWinTest : public InProcessBrowserTest {
 protected:
  SandboxWinTest() : InProcessBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kTestSandbox,
                                    "security_tests.dll");
  }
};

// Need a cross-platform test library: http://crbug.com/45771
// Verifies that chrome is running properly.
IN_PROC_BROWSER_TEST_F(SandboxWinTest, ExecuteDll) {
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}
