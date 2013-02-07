// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

class ExperimentalApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

IN_PROC_BROWSER_TEST_F(ExperimentalApiTest, AppNotificationIncognito) {
  ASSERT_TRUE(RunExtensionSubtest(
      "app_notifications", "test.html",
      ExtensionApiTest::kFlagEnableIncognito |
      ExtensionApiTest::kFlagUseIncognito)) << message_;
}
