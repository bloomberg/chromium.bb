// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "ui/base/l10n/l10n_util.h"

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
    // The only guarantee we can make about the title of a settings tab is that
    // it should contain IDS_SETTINGS_TITLE somewhere.
    ASSERT_FALSE(WideToUTF16Hack(title).find(expected_title) == string16::npos);
  }
};

// Flaky: http://crbug.com/77375
TEST_F(OptionsUITest, FLAKY_LoadOptionsByURL) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab = browser->GetActiveTab();
  ASSERT_TRUE(tab.get());

  // Go to the options tab via URL.
  NavigateToURL(GURL(chrome::kChromeUISettingsURL));
  AssertIsOptionsPage(tab);
}

// Flaky, and takes very long to fail. http://crbug.com/64619.
TEST_F(OptionsUITest, DISABLED_CommandOpensOptionsTab) {
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

// Flaky, and takes very long to fail. http://crbug.com/48521
TEST_F(OptionsUITest, DISABLED_CommandAgainGoesBackToOptionsTab) {
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

// Flaky, and takes very long to fail. http://crbug.com/48521
TEST_F(OptionsUITest, DISABLED_TwoCommandsOneTab) {
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
