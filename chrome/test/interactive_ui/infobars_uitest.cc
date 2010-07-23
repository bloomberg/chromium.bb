// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/extension_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"

class InfoBarsUITest : public UITest {
 public:
  InfoBarsUITest() {
    show_window_ = true;
  }
};

TEST_F(InfoBarsUITest, TestInfoBarsCloseOnNewTheme) {
  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server(HTTPTestServer::CreateServer(kDocRoot));
  ASSERT_TRUE(server.get() != NULL);
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab_1(browser->GetActiveTab());
  ASSERT_TRUE(tab_1.get());
  EXPECT_TRUE(tab_1->NavigateToURL(
      server->TestServerPage("files/simple.html")));
  scoped_refptr<ExtensionProxy> theme = automation()->InstallExtension(
      test_data_directory_.AppendASCII("extensions").AppendASCII("theme.crx"),
      true);
  ASSERT_TRUE(theme != NULL);
  EXPECT_TRUE(tab_1->WaitForInfoBarCount(1, action_max_timeout_ms()));

  EXPECT_TRUE(browser->AppendTab(
      server->TestServerPage("files/simple.html")));
  WaitUntilTabCount(2);
  scoped_refptr<TabProxy> tab_2(browser->GetActiveTab());
  ASSERT_TRUE(tab_2.get());
  theme = automation()->InstallExtension(
      test_data_directory_.AppendASCII("extensions").AppendASCII("theme2.crx"),
      true);
  ASSERT_TRUE(theme != NULL);
  EXPECT_TRUE(tab_2->WaitForInfoBarCount(1, action_max_timeout_ms()));
  EXPECT_TRUE(tab_1->WaitForInfoBarCount(0, action_max_timeout_ms()));

  EXPECT_TRUE(automation()->ResetToDefaultTheme());
  EXPECT_TRUE(tab_2->WaitForInfoBarCount(0, action_max_timeout_ms()));
}
