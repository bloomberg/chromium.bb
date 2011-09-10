// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
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

TEST_F(OptionsUITest, LoadOptionsByURL) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab = browser->GetActiveTab();
  ASSERT_TRUE(tab.get());

  NavigateToURL(GURL(chrome::kChromeUISettingsURL));
  AssertIsOptionsPage(tab);

  // Check navbar's existence.
  bool navbar_exist = false;
  ASSERT_TRUE(tab->ExecuteAndExtractBool(L"",
      L"domAutomationController.send("
      L"!!document.getElementById('navbar'))", &navbar_exist));
  ASSERT_EQ(true, navbar_exist);

  // Check section headers in navbar.
  // For ChromeOS, there should be 1 + 7:
  //   Search, Basics, Personal, System, Internet, Under the Hood,
  //   Users and Extensions.
  // For other platforms, there should 1 + 4:
  //   Search, Basics, Personal, Under the Hood and Extensions.
#if defined(OS_CHROMEOS)
  const int kExpectedSections = 1 + 7;
#else
  const int kExpectedSections = 1 + 4;
#endif
  int num_of_sections = 0;
  ASSERT_TRUE(tab->ExecuteAndExtractInt(L"",
      L"domAutomationController.send("
      L"document.getElementById('navbar').children.length)", &num_of_sections));
  ASSERT_EQ(kExpectedSections, num_of_sections);
}

}  // namespace
