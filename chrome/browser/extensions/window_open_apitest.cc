// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FLAKY_WindowOpen) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ResultCatcher catcher;
  ASSERT_TRUE(LoadExtensionIncognito(test_data_dir_
      .AppendASCII("window_open").AppendASCII("spanning")));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

void WaitForTabsAndPopups(Browser* browser, int num_tabs, int num_popups) {
  // We start with one tab and one browser already open.
  ++num_tabs;
  size_t num_browsers = static_cast<size_t>(num_popups) + 1;

  while (true) {
    if (BrowserList::GetBrowserCount(browser->profile()) < num_browsers ||
        browser->tab_count() < num_tabs) {
      MessageLoopForUI::current()->RunAllPending();
      continue;
    }

    ASSERT_EQ(num_browsers, BrowserList::GetBrowserCount(browser->profile()));
    ASSERT_EQ(num_tabs, browser->tab_count());

    for (BrowserList::const_iterator iter = BrowserList::begin();
         iter != BrowserList::end(); ++iter) {
      if (*iter == browser)
        continue;

      ASSERT_EQ(Browser::TYPE_POPUP, (*iter)->type());
    }

    break;
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PopupBlockingExtension) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup_blocking")
      .AppendASCII("extension")));

  WaitForTabsAndPopups(browser(), 5, 3);
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PopupBlockingHostedApp) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup_blocking")
      .AppendASCII("hosted_app")));

  std::string app_base("http://a.com:1337/files/extensions/api_test/"
                       "window_open/popup_blocking/hosted_app/");
  browser()->OpenURL(GURL(app_base + "open_tab.html"), GURL(),
                     NEW_FOREGROUND_TAB, PageTransition::TYPED);
  browser()->OpenURL(GURL(app_base + "open_popup.html"), GURL(),
                     NEW_FOREGROUND_TAB, PageTransition::TYPED);

  WaitForTabsAndPopups(browser(), 3, 1);
}
