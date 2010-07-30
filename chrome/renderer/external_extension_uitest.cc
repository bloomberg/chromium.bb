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
  virtual ~SearchProviderTest();

  void TestIsSearchProviderInstalledForHost(
      TabProxy* tab,
      const char* host,
      const char* expected_result);

  scoped_refptr<net::HTTPTestServer> server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchProviderTest);
};

SearchProviderTest::SearchProviderTest()
    : server_(net::HTTPTestServer::CreateServer(L"chrome/test/data")) {
  if (!server_)
    return;

  // Enable the search provider additions.
  launch_arguments_.AppendSwitch(switches::kEnableSearchProviderApiV2);

  // Map all hosts to our local server.
  GURL server_url = server_->TestServerPage("");
  std::string host_rule = "MAP * ";
  host_rule.append(server_url.host());
  if (server_url.has_port()) {
    host_rule.append(":");
    host_rule.append(server_url.port());
  }
  launch_arguments_.AppendSwitchASCII(switches::kHostRules, host_rule);
}

SearchProviderTest::~SearchProviderTest() {
  server_->Stop();
}

void SearchProviderTest::TestIsSearchProviderInstalledForHost(
    TabProxy* tab,
    const char* host,
    const char* expected_result) {
  ASSERT_TRUE(server_);
  GURL local_url =
      server_->TestServerPage("files/is_search_provider_installed.html");
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

TEST_F(SearchProviderTest, DISABLED_TestIsSearchProviderInstalled) {
  // Verify the default search provider, other installed search provider, and
  // one not installed as well.
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
