// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/test_server.h"

namespace {

struct IsSearchProviderTestData {
  IsSearchProviderTestData() : tab(NULL) {}
  IsSearchProviderTestData(content::WebContents* t, std::string h, GURL url)
      : tab(t), host(h), test_url(url) {
  }

  content::WebContents* tab;
  std::string host;
  GURL test_url;
};

}

class SearchProviderTest : public InProcessBrowserTest {
 protected:
  SearchProviderTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ASSERT_TRUE(test_server()->Start());

    // Map all hosts to our local server.
    std::string host_rule(
        "MAP * " + test_server()->host_port_pair().ToString());
    command_line->AppendSwitchASCII(switches::kHostRules, host_rule);
    // Use no proxy or otherwise this test will fail on a machine that has a
    // proxy configured.
    command_line->AppendSwitch(switches::kNoProxyServer);

    // Get the url for the test page.
    search_provider_test_url_ =
        test_server()->GetURL("files/is_search_provider_installed.html");
  }

  IsSearchProviderTestData StartIsSearchProviderInstalledTest(
      Browser* browser,
      const char* host,
      const char* expected_result) {
    GURL test_url(std::string("http://") + host +
        search_provider_test_url_.path() + "#" + expected_result);
    ui_test_utils::NavigateToURLWithDisposition(
        browser, test_url, NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

    // Bundle up information needed to verify the result.
    content::WebContents* tab =
        browser->tab_strip_model()->GetActiveWebContents();
    return IsSearchProviderTestData(tab, host, test_url);
  }

  void FinishIsSearchProviderInstalledTest(
      const IsSearchProviderTestData& data) {
    string16 title = data.tab->GetTitle();
    if (title.empty()) {
      content::TitleWatcher title_watcher(data.tab, ASCIIToUTF16("OK"));
      title_watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));
      title = title_watcher.WaitAndGetTitle();
    }
    EXPECT_EQ(ASCIIToUTF16("OK"), title);
  }

  GURL search_provider_test_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchProviderTest);
};

#if defined(OS_WIN)
// This is flaking on XP. See http://crbug.com/159530
#define MAYBE_TestIsSearchProviderInstalled \
    DISABLED_TestIsSearchProviderInstalled
#else
#define MAYBE_TestIsSearchProviderInstalled TestIsSearchProviderInstalled
#endif
IN_PROC_BROWSER_TEST_F(SearchProviderTest,
                       MAYBE_TestIsSearchProviderInstalled) {
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
  for (size_t i = 0; i < arraysize(test_hosts); ++i) {
    test_data[i] = StartIsSearchProviderInstalledTest(
        browser(), test_hosts[i], expected_results[i]);
    FinishIsSearchProviderInstalledTest(test_data[i]);
  }

  // Start tests for incognito mode (and verify the result is 0).
  Browser* incognito_browser = CreateIncognitoBrowser();
  for (size_t i = 0; i < arraysize(test_hosts); ++i) {
    test_data[i + arraysize(test_hosts)] = StartIsSearchProviderInstalledTest(
        incognito_browser, test_hosts[i], "0");
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

IN_PROC_BROWSER_TEST_F(SearchProviderTest,
                       TestIsSearchProviderInstalledWithException) {
  // Change the url for the test page to one that throws an exception when
  // toString is called on the argument given to isSearchProviderInstalled.
  search_provider_test_url_ = test_server()->GetURL(
      "files/is_search_provider_installed_with_exception.html");

  FinishIsSearchProviderInstalledTest(StartIsSearchProviderInstalledTest(
      browser(), "www.google.com", ""));
}
