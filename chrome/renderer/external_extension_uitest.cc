// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_layout_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/escape.h"
#include "net/test/test_server.h"

class SearchProviderTest : public UITest {
 protected:
  SearchProviderTest();

  void TestIsSearchProviderInstalledForHost(
      TabProxy* tab,
      const char* host,
      const char* expected_result);

  net::TestServer test_server_;
};

SearchProviderTest::SearchProviderTest()
    : test_server_(net::TestServer::TYPE_HTTP,
                   FilePath(FILE_PATH_LITERAL("chrome/test/data"))) {
  // Enable the search provider additions.
  launch_arguments_.AppendSwitch(switches::kEnableSearchProviderApiV2);

  // Map all hosts to our local server.
  std::string host_rule("MAP * " + test_server_.host_port_pair().ToString());
  launch_arguments_.AppendSwitchASCII(switches::kHostRules, host_rule);
}

void SearchProviderTest::TestIsSearchProviderInstalledForHost(
    TabProxy* tab,
    const char* host,
    const char* expected_result) {
  GURL local_url =
      test_server_.GetURL("files/is_search_provider_installed.html");
  GURL test_url(std::string("http://") + host + local_url.path() +
                "#" + expected_result);
  EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(test_url));
  std::string cookie_name = std::string(host) + "testResult";
  std::string escaped_value =
      WaitUntilCookieNonEmpty(tab, test_url,
                              cookie_name.c_str(), action_max_timeout_ms());

  // Unescapes and normalizes the actual result.
  std::string value = UnescapeURLComponent(escaped_value,
      UnescapeRule::NORMAL | UnescapeRule::SPACES |
          UnescapeRule::URL_SPECIAL_CHARS | UnescapeRule::CONTROL_CHARS);
  value += "\n";
  ReplaceSubstringsAfterOffset(&value, 0, "\r", "");
  EXPECT_STREQ("1\n", value.c_str());
}

// Verify the default search provider, other installed search provider, and
// one not installed as well.
TEST_F(SearchProviderTest, DISABLED_TestIsSearchProviderInstalled) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  TestIsSearchProviderInstalledForHost(tab, "www.google.com", "2");
  TestIsSearchProviderInstalledForHost(tab, "www.bing.com", "1");
  TestIsSearchProviderInstalledForHost(tab, "localhost", "0");

  // Verify that there are no search providers reported in incognito mode.
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  ASSERT_TRUE(browser->RunCommand(IDC_NEW_INCOGNITO_WINDOW));
  scoped_refptr<BrowserProxy> incognito(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(incognito.get());
  scoped_refptr<TabProxy> incognito_tab(incognito->GetTab(0));
  ASSERT_TRUE(incognito_tab.get());
  TestIsSearchProviderInstalledForHost(incognito_tab, "www.google.com", "0");
  TestIsSearchProviderInstalledForHost(incognito_tab, "www.bing.com", "0");
  TestIsSearchProviderInstalledForHost(incognito_tab, "localhost", "0");
}
