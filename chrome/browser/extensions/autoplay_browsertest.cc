// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "media/base/media_switches.h"

class AutoplayExtensionBrowserTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kDocumentUserActivationRequiredPolicy);
  }
};

IN_PROC_BROWSER_TEST_F(AutoplayExtensionBrowserTest, AutoplayAllowed) {
  ASSERT_TRUE(RunExtensionTest("autoplay")) << message_;
}
