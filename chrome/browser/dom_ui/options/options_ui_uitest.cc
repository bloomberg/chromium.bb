// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/string16.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"

#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

class OptionsUITest : public UITest {
 public:
  OptionsUITest() {
    dom_automation_enabled_ = true;
  }

  void AssertIsOptionsPage(TabProxy* tab) {
    std::wstring title;
    ASSERT_TRUE(tab->GetTabTitle(&title));
    string16 expected_title = l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE);
    ASSERT_EQ(expected_title, WideToUTF16Hack(title));
  }
};

TEST_F(OptionsUITest, LoadOptionsByURL) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab = browser->GetActiveTab();
  ASSERT_TRUE(tab.get());

  // Go to the options tab via URL.
  NavigateToURL(GURL(chrome::kChromeUISettingsURL));
  AssertIsOptionsPage(tab);
}

TEST_F(OptionsUITest, CommandOpensOptionsTab) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  int tab_count = -1;
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(1, tab_count);

  // Bring up the options tab via command.
  ASSERT_TRUE(browser->RunCommand(IDC_OPTIONS));
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(2, tab_count);

  scoped_refptr<TabProxy> tab = browser->GetActiveTab();
  ASSERT_TRUE(tab.get());
  AssertIsOptionsPage(tab);
}

// TODO(csilv): Investigate why this fails and fix. http://crbug.com/48521
// Also, crashing on linux/views.
#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
#define MAYBE_CommandAgainGoesBackToOptionsTab \
    DISABLED_CommandAgainGoesBackToOptionsTab
#else
#define MAYBE_CommandAgainGoesBackToOptionsTab \
    FLAKY_CommandAgainGoesBackToOptionsTab
#endif

TEST_F(OptionsUITest, MAYBE_CommandAgainGoesBackToOptionsTab) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  int tab_count = -1;
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(1, tab_count);

  // Bring up the options tab via command.
  ASSERT_TRUE(browser->RunCommand(IDC_OPTIONS));
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(2, tab_count);

  scoped_refptr<TabProxy> tab = browser->GetActiveTab();
  ASSERT_TRUE(tab.get());
  AssertIsOptionsPage(tab);

  // Switch to first tab and run command again.
  ASSERT_TRUE(browser->ActivateTab(0));
  ASSERT_TRUE(browser->WaitForTabToBecomeActive(
      0, TestTimeouts::action_max_timeout_ms()));
  ASSERT_TRUE(browser->RunCommand(IDC_OPTIONS));

  // Ensure the options ui tab is active.
  ASSERT_TRUE(browser->WaitForTabToBecomeActive(
      1, TestTimeouts::action_max_timeout_ms()));
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(2, tab_count);
}

// TODO(csilv): Investigate why this fails (sometimes) on 10.5 and fix.
// http://crbug.com/48521. Also, crashing on linux/views.
#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
#define MAYBE_TwoCommandsOneTab DISABLED_TwoCommandsOneTab
#else
#define MAYBE_TwoCommandsOneTab FLAKY_TwoCommandsOneTab
#endif

TEST_F(OptionsUITest, MAYBE_TwoCommandsOneTab) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  int tab_count = -1;
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(1, tab_count);

  ASSERT_TRUE(browser->RunCommand(IDC_OPTIONS));
  ASSERT_TRUE(browser->RunCommand(IDC_OPTIONS));
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(2, tab_count);
}

}  // namespace
