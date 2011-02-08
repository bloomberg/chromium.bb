// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class PrintPreviewUITest : public UITest {
 public:
  PrintPreviewUITest() {
    dom_automation_enabled_ = true;
    // TODO(thestig): Remove when print preview is enabled by default.
    launch_arguments_.AppendSwitch(switches::kEnablePrintPreview);
  }

  void AssertIsPrintPage(TabProxy* tab) {
    std::wstring title;
    ASSERT_TRUE(tab->GetTabTitle(&title));
    string16 expected_title =
        l10n_util::GetStringUTF16(IDS_PRINT_PREVIEW_TITLE);
    ASSERT_EQ(expected_title, WideToUTF16Hack(title));
  }
};

// TODO(thestig) Remove this test in the future if loading
// chrome::kChromeUIPrintURL directly does not make sense.
TEST_F(PrintPreviewUITest, LoadPrintPreviewByURL) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab = browser->GetActiveTab();
  ASSERT_TRUE(tab.get());

  // Go to the print preview tab via URL.
  NavigateToURL(GURL(chrome::kChromeUIPrintURL));
  AssertIsPrintPage(tab);
}

// Flaky on interactive tests builder. See http://crbug.com/69389
TEST_F(PrintPreviewUITest, FLAKY_PrintCommandDisabled) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  // Go to the about:blank page.
  NavigateToURL(GURL(chrome::kAboutBlankURL));

  // Make sure there is 1 tab and print is enabled. Create print preview tab.
  int tab_count;
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(1, tab_count);
  bool enabled;
  ASSERT_TRUE(browser->IsMenuCommandEnabled(IDC_PRINT, &enabled));
  ASSERT_TRUE(enabled);
  ASSERT_TRUE(browser->RunCommand(IDC_PRINT));

  // Make sure there are 2 tabs and print is disabled.
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(2, tab_count);
  scoped_refptr<TabProxy> tab = browser->GetActiveTab();
  ASSERT_TRUE(tab.get());
  AssertIsPrintPage(tab);
  ASSERT_TRUE(browser->IsMenuCommandEnabled(IDC_PRINT, &enabled));
  ASSERT_FALSE(enabled);
}

}  // namespace
