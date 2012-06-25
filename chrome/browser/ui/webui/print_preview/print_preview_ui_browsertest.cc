// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/test_navigation_observer.h"

namespace {

class PrintPreviewTest : public InProcessBrowserTest {
 public:
  PrintPreviewTest() {
    EnableDOMAutomation();
  }

#if !defined(GOOGLE_CHROME_BUILD)
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnablePrintPreview);
  }
#endif

  void Print() {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
        content::NotificationService::AllSources());
    browser()->ExecuteCommand(IDC_PRINT);
    observer.Wait();
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, PrintCommands) {
  // We start off at about:blank page.
  // Make sure there is 1 tab and print is enabled.
  ASSERT_EQ(1, browser()->tab_count());
  ASSERT_TRUE(browser()->command_updater()->IsCommandEnabled(IDC_PRINT));

  // Make sure advanced print command (Ctrl+Shift+p) is enabled.
  ASSERT_TRUE(
      browser()->command_updater()->IsCommandEnabled(IDC_ADVANCED_PRINT));

  // Create print preview tab.
  Print();

  // Make sure print is disabled.
  ASSERT_FALSE(browser()->command_updater()->IsCommandEnabled(IDC_PRINT));

  // Make sure advanced print command (Ctrl+Shift+p) is enabled.
  ASSERT_TRUE(
      browser()->command_updater()->IsCommandEnabled(IDC_ADVANCED_PRINT));

  content::TestNavigationObserver reload_observer(
      content::NotificationService::AllSources());
  chrome::Reload(browser(), CURRENT_TAB);
  reload_observer.Wait();

  ASSERT_TRUE(browser()->command_updater()->IsCommandEnabled(IDC_PRINT));

  // Make sure advanced print command (Ctrl+Shift+p) is enabled.
  ASSERT_TRUE(
      browser()->command_updater()->IsCommandEnabled(IDC_ADVANCED_PRINT));
}

}  // namespace
