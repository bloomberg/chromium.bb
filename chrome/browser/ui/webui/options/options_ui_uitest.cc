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
#include "chrome/test/automation/automation_proxy.h"
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

  bool WaitForOptionsUI(TabProxy* tab) {
    return WaitUntilJavaScriptCondition(tab, L"",
        L"domAutomationController.send("
        L"    location.protocol == 'chrome:' && "
        L"    document.readyState == 'complete')",
        TestTimeouts::large_test_timeout_ms());
  }

  scoped_refptr<TabProxy> GetOptionsUITab() {
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    EXPECT_TRUE(browser.get());
    if (!browser.get())
      return NULL;
    scoped_refptr<TabProxy> tab = browser->GetActiveTab();
    EXPECT_TRUE(tab.get());
    if (!tab.get())
      return NULL;
    bool success = tab->NavigateToURL(GURL(chrome::kChromeUISettingsURL));
    EXPECT_TRUE(success);
    if (!success)
      return NULL;
    success = WaitForOptionsUI(tab);
    EXPECT_TRUE(success);
    if (!success)
      return NULL;
    return tab;
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

TEST_F(OptionsUITest, LoadOptionsByURL) {
  scoped_refptr<TabProxy> tab = GetOptionsUITab();
  ASSERT_TRUE(tab.get());
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

// Navigates to settings page and do sanity check on settings sections.
TEST_F(OptionsUITest, NavBarCheck) {
  scoped_refptr<TabProxy> tab = GetOptionsUITab();
  ASSERT_TRUE(tab.get());
  AssertIsOptionsPage(tab);

  // Check navbar's existence.
  bool navbar_exist = false;
  ASSERT_TRUE(tab->ExecuteAndExtractBool(L"",
      L"domAutomationController.send("
      L"!!document.getElementById('navbar'))", &navbar_exist));
  ASSERT_EQ(true, navbar_exist);

  // Check section headers in navbar.
  // For ChromeOS, there should be 1 + 6:
  //   search, basics, personal, systerm, internet, under the hood and users
  // For other platforms, there should 1 + 3:
  //   search, basics, personal and under the hood.
#if defined(OS_CHROMEOS)
  const int kExpectedSections = 1 + 6;
#else
  const int kExpectedSections = 1 + 3;
#endif
  int num_of_sections = 0;
  ASSERT_TRUE(tab->ExecuteAndExtractInt(L"",
      L"domAutomationController.send("
      L"document.getElementById('navbar').children.length)", &num_of_sections));
  ASSERT_EQ(kExpectedSections, num_of_sections);
}

}  // namespace
