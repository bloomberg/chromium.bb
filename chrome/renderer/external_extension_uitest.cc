// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_timeouts.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_layout_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/escape.h"
#include "net/test/test_server.h"

struct IsSearchProviderTestData;

class SearchProviderTest : public UITest {
 protected:
  SearchProviderTest();

  IsSearchProviderTestData StartIsSearchProviderInstalledTest(
      BrowserProxy* browser_proxy,
      const char* host,
      const char* expected_result);

  void FinishIsSearchProviderInstalledTest(
      const IsSearchProviderTestData& data);

  net::TestServer test_server_;
  bool test_server_started_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchProviderTest);
};

SearchProviderTest::SearchProviderTest()
    : test_server_(net::TestServer::TYPE_HTTP,
                   FilePath(FILE_PATH_LITERAL("chrome/test/data"))),
      test_server_started_(false) {
  // The test_server is started in the constructor (rather than the test body)
  // so the mapping rules below can include the ephemeral port number.
  test_server_started_ = test_server_.Start();
  if (!test_server_started_)
    return;

  // Enable the search provider additions.
  launch_arguments_.AppendSwitch(switches::kEnableSearchProviderApiV2);

  // Map all hosts to our local server.
  std::string host_rule("MAP * " + test_server_.host_port_pair().ToString());
  launch_arguments_.AppendSwitchASCII(switches::kHostRules, host_rule);
}

struct IsSearchProviderTestData {
  IsSearchProviderTestData() {
  }

  IsSearchProviderTestData(TabProxy* t,
                           std::string h,
                           GURL url)
      : tab(t),
        host(h),
        test_url(url) {
  }

  scoped_refptr<TabProxy> tab;
  std::string host;
  GURL test_url;
};

IsSearchProviderTestData SearchProviderTest::StartIsSearchProviderInstalledTest(
    BrowserProxy* browser_proxy,
    const char* host,
    const char* expected_result) {
  // Set-up a new tab for the navigation.
  int num_tabs = 0;
  if (!browser_proxy->GetTabCount(&num_tabs)) {
    ADD_FAILURE() << "BrowserProxy::GetTabCount failed.";
    return IsSearchProviderTestData();
  }

  GURL blank(chrome::kAboutBlankURL);
  if (!browser_proxy->AppendTab(blank)) {
    ADD_FAILURE() << "BrowserProxy::AppendTab failed.";
    return IsSearchProviderTestData();
  }

  scoped_refptr<TabProxy> tab(browser_proxy->GetTab(num_tabs));
  if (!tab.get()) {
    ADD_FAILURE() << "BrowserProxy::GetTab for the new tab failed.";
    return IsSearchProviderTestData();
  }

  // Go to the test page.
  GURL local_url =
      test_server_.GetURL("files/is_search_provider_installed.html");
  GURL test_url(std::string("http://") + host + local_url.path() +
                "#" + expected_result);
  EXPECT_TRUE(tab->NavigateToURLAsync(test_url));

  // Bundle up information needed to verify the result.
  return IsSearchProviderTestData(tab, host, test_url);
}

void SearchProviderTest::FinishIsSearchProviderInstalledTest(
    const IsSearchProviderTestData& data) {
  ASSERT_TRUE(data.tab.get());

  std::string cookie_name = data.host + "testResult";
  std::string escaped_value =
      WaitUntilCookieNonEmpty(data.tab,
                              data.test_url,
                              cookie_name.c_str(),
                              TestTimeouts::action_max_timeout_ms());

  // Unescapes and normalizes the actual result.
  std::string value = UnescapeURLComponent(
      escaped_value,
      UnescapeRule::NORMAL | UnescapeRule::SPACES |
      UnescapeRule::URL_SPECIAL_CHARS | UnescapeRule::CONTROL_CHARS);
  value += "\n";
  ReplaceSubstringsAfterOffset(&value, 0, "\r", "");
  EXPECT_STREQ("1\n", value.c_str());
}

// http://code.google.com/p/chromium/issues/detail?id=62777
TEST_F(SearchProviderTest, FLAKY_TestIsSearchProviderInstalled) {
  ASSERT_TRUE(test_server_started_);

  // Use the default search provider, other installed search provider, and
  // one not installed as well. (Note that yahoo isn't tested because the
  // its host name varies a lot for different locales unlike Google and Bing,
  // which would make the test fail depending on the machine's locale.)
  const char* test_hosts[] = { "www.google.com",
                               "www.bing.com",
                               "localhost" };
  const char* expected_results[] = { "2",
                                     "1",
                                     "0" };
  COMPILE_ASSERT(arraysize(test_hosts) == arraysize(expected_results),
                 there_should_be_a_result_for_each_host);
  IsSearchProviderTestData test_data[2 * arraysize(test_hosts)];

  // Start results for the normal mode.
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  for (size_t i = 0; i < arraysize(test_hosts); ++i) {
    test_data[i] = StartIsSearchProviderInstalledTest(
        browser, test_hosts[i], expected_results[i]);
    FinishIsSearchProviderInstalledTest(test_data[i]);
  }

  // Start tests for incognito mode (and verify the result is 0).
  ASSERT_TRUE(browser->RunCommand(IDC_NEW_INCOGNITO_WINDOW));
  scoped_refptr<BrowserProxy> incognito(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(incognito.get());
  for (size_t i = 0; i < arraysize(test_hosts); ++i) {
    test_data[i + arraysize(test_hosts)] = StartIsSearchProviderInstalledTest(
        incognito, test_hosts[i], "0");
    FinishIsSearchProviderInstalledTest(test_data[i + arraysize(test_hosts)]);
  }

  // The following should be re-enabled. At the moment, there are problems with
  // doing all of these queries in parallel -- see http://crbug.com/60043.
#if 0
  // Remove the calls to FinishIsSearchProviderInstalledTest above when
  // re-enabling this code.

  // Do the verification.
  for (size_t i = 0; i < arraysize(test_data); ++i) {
    FinishIsSearchProviderInstalledTest(test_data[i]);
  }
#endif
}
