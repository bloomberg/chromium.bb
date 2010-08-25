// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"

class AppBackgroundPageApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisablePopupBlocking);
  }
};

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, Basic) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  LoadExtension(test_data_dir_.AppendASCII(
      "app_background_page/app_has_permission"));
  ASSERT_TRUE(RunExtensionTest("app_background_page/basic")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, LacksPermission) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  LoadExtension(test_data_dir_.AppendASCII(
      "app_background_page/app_lacks_permission"));
  ASSERT_TRUE(RunExtensionTest("app_background_page/lacks_permission"))
      << message_;
}
