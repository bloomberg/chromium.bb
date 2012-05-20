// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

// Offscreen Tabs does not work on ChromeOS right now.
#if !defined(OS_CHROMEOS)

class OffscreenTabsApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

IN_PROC_BROWSER_TEST_F(OffscreenTabsApiTest, DISABLED_OffscreenTabBasics) {
  ASSERT_TRUE(RunExtensionSubtest("offscreen_tabs", "crud.html")) << message_;
}


IN_PROC_BROWSER_TEST_F(OffscreenTabsApiTest, OffscreenTabMouseEvents) {
  ASSERT_TRUE(RunExtensionSubtest("offscreen_tabs",
                                  "mouse_events.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(OffscreenTabsApiTest, OffscreenTabKeyboardEvents) {
  ASSERT_TRUE(RunExtensionSubtest("offscreen_tabs",
                                  "keyboard_events.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(OffscreenTabsApiTest, OffscreenTabDisplay) {
  ASSERT_TRUE(RunExtensionSubtest("offscreen_tabs",
                                  "display.html")) << message_;
}

#endif  // #if !defined(OS_CHROMEOS)
