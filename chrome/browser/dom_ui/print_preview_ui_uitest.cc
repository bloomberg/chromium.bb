// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"

#include "grit/generated_resources.h"

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

}  // namespace
