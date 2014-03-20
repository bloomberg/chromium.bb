// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class AutomationApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableAutomationAPI);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }
};

// http://crbug.com/353039
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DISABLED_SanityCheck) {
  ASSERT_TRUE(RunComponentExtensionTest("automation/sanity_check")) << message_;
}

}  // namespace extensions
