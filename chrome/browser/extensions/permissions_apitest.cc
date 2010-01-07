// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

class ExperimentalApiTest : public ExtensionApiTest {
public:
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PermissionsFail) {
  ASSERT_TRUE(RunExtensionTest("permissions/disabled")) << message_;

  // Since the experimental APIs require a flag, this will fail even though
  // It's enabled.
  // TODO(erikkay) This test is currently broken because LoadExtension in
  // ExtensionBrowserTest doesn't actually fail, it just times out.  To fix this
  // I'll need to add an EXTENSION_LOAD_ERROR notification, which is probably
  // too much for the branch.  I'll enable this on trunk later.
  //ASSERT_FALSE(RunExtensionTest("permissions/enabled"))) << message_;
}

IN_PROC_BROWSER_TEST_F(ExperimentalApiTest, PermissionsSucceed) {
  ASSERT_TRUE(RunExtensionTest("permissions/enabled")) << message_;
}

