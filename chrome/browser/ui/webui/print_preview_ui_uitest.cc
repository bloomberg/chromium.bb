// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_proxy.h"
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
    launch_arguments_.AppendSwitch(switches::kEnablePrintPreview);
  }
};

TEST_F(PrintPreviewUITest, PrintCommands) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  // Go to the about:blank page.
  NavigateToURL(GURL(chrome::kAboutBlankURL));

  // Make sure there is 1 tab and print is enabled.
  int tab_count;
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(1, tab_count);

  bool enabled = false;
  ASSERT_TRUE(browser->IsMenuCommandEnabled(IDC_PRINT, &enabled));
  ASSERT_TRUE(enabled);

  // Make sure advanced print command (Ctrl+Shift+p) is enabled.
  enabled = false;
  ASSERT_TRUE(browser->IsMenuCommandEnabled(IDC_ADVANCED_PRINT, &enabled));
  ASSERT_TRUE(enabled);

  // Create print preview tab.
  ASSERT_TRUE(browser->RunCommand(IDC_PRINT));

  // Make sure print is disabled.
  ASSERT_TRUE(browser->IsMenuCommandEnabled(IDC_PRINT, &enabled));
  ASSERT_FALSE(enabled);

  // Make sure advanced print command (Ctrl+Shift+p) is enabled.
  enabled = false;
  ASSERT_TRUE(browser->IsMenuCommandEnabled(IDC_ADVANCED_PRINT, &enabled));
  ASSERT_TRUE(enabled);

  ASSERT_TRUE(browser->RunCommand(IDC_RELOAD));

  enabled = false;
  ASSERT_TRUE(browser->IsMenuCommandEnabled(IDC_PRINT, &enabled));
  ASSERT_TRUE(enabled);

  // Make sure advanced print command (Ctrl+Shift+p) is enabled.
  enabled = false;
  ASSERT_TRUE(browser->IsMenuCommandEnabled(IDC_ADVANCED_PRINT, &enabled));
  ASSERT_TRUE(enabled);
}

}  // namespace
