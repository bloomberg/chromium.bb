// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/common/chrome_switches.h"


class ExtensionManagementApiBrowserTest : public ExtensionBrowserTest {
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalExtensionApis);
  }
};

// We test this here instead of in an ExtensionApiTest because normal extensions
// are not allowed to call the install function.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest, InstallEvent) {
  ExtensionTestMessageListener listener1("ready");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/install_event")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());

  ExtensionTestMessageListener listener2("got_event");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/management/enabled_extension")));
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest, LaunchApp) {
  ExtensionTestMessageListener listener1("app_launched");
  ExtensionTestMessageListener listener2("got_expected_error");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/simple_extension")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/packaged_app")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_app")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}
