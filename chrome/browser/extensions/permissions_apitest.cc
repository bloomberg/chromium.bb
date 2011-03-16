// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
  // it's enabled.
  // TODO(erikkay) This test is currently broken because LoadExtension in
  // ExtensionBrowserTest doesn't actually fail, it just times out.  To fix this
  // I'll need to add an EXTENSION_LOAD_ERROR notification, which is probably
  // too much for the branch.  I'll enable this on trunk later.
  //ASSERT_FALSE(RunExtensionTest("permissions/enabled"))) << message_;
}

IN_PROC_BROWSER_TEST_F(ExperimentalApiTest, PermissionsSucceed) {
  ASSERT_TRUE(RunExtensionTest("permissions/enabled")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ExperimentalPermissionsFail) {
  // At the time this test is being created, there is no experimental
  // function that will not be graduating soon, and does not require a
  // tab id as an argument.  So, we need the tab permission to get
  // a tab id.
  ASSERT_TRUE(RunExtensionTest("permissions/experimental_disabled"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FaviconPermission) {
  ASSERT_TRUE(RunExtensionTest("permissions/favicon")) << message_;
}

// Test functions and APIs that are always allowed (even if you ask for no
// permissions).
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, AlwaysAllowed) {
  ASSERT_TRUE(RunExtensionTest("permissions/always_allowed")) << message_;
}
