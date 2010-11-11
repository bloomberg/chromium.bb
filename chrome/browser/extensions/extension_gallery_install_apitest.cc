// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_webstore_private_api.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"

class ExtensionGalleryInstallApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kAppsGalleryURL,
        "http://www.example.com");
  }

  bool RunInstallTest(const std::string& page) {
    std::string base_url = base::StringPrintf(
        "http://www.example.com:%u/files/extensions/",
        test_server()->host_port_pair().port());

    std::string testing_install_base_url = base_url;
    testing_install_base_url += "good.crx";
    CompleteInstallFunction::SetTestingInstallBaseUrl(
        testing_install_base_url.c_str());

    std::string page_url = base_url;
    page_url += "api_test/extension_gallery_install/" + page;

    return RunPageTest(page_url.c_str());
  }
};

// http://crbug.com/55642 - failing on XP.
#if defined (OS_WIN)
#define MAYBE_InstallAndUninstall DISABLED_InstallAndUninstall
#else
#define MAYBE_InstallAndUninstall InstallAndUninstall
#endif
IN_PROC_BROWSER_TEST_F(ExtensionGalleryInstallApiTest,
                       MAYBE_InstallAndUninstall) {
  host_resolver()->AddRule("www.example.com", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  BeginInstallFunction::SetIgnoreUserGestureForTests(true);
  ASSERT_TRUE(RunInstallTest("test.html"));
  ASSERT_TRUE(RunInstallTest("complete_without_begin.html"));
  ASSERT_TRUE(RunInstallTest("invalid_begin.html"));

  BeginInstallFunction::SetIgnoreUserGestureForTests(false);
  ASSERT_TRUE(RunInstallTest("no_user_gesture.html"));
}
