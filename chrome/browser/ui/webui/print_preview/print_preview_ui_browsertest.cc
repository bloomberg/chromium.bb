// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "url/url_constants.h"

#if defined(OS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

using task_manager::browsertest_util::MatchAboutBlankTab;
using task_manager::browsertest_util::MatchAnyPrint;
using task_manager::browsertest_util::MatchAnyTab;
using task_manager::browsertest_util::MatchPrint;
using task_manager::browsertest_util::WaitForTaskManagerRows;

namespace {

class PrintPreviewTest : public InProcessBrowserTest {
 public:
  PrintPreviewTest() {}

  void Print() {
    content::TestNavigationObserver nav_observer(NULL);
    nav_observer.StartWatchingNewWebContents();
    chrome::ExecuteCommand(browser(), IDC_PRINT);
    nav_observer.Wait();
    nav_observer.StopWatchingNewWebContents();
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, PrintCommands) {
  // We start off at about:blank page.
  // Make sure there is 1 tab and print is enabled.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  ASSERT_TRUE(chrome::IsCommandEnabled(browser(), IDC_PRINT));

#if defined(DISABLE_BASIC_PRINTING)
  bool is_basic_print_expected = false;
#else
  bool is_basic_print_expected = true;
#endif  // DISABLE_BASIC_PRINTING

  ASSERT_EQ(is_basic_print_expected,
            chrome::IsCommandEnabled(browser(), IDC_BASIC_PRINT));

  // Create the print preview dialog.
  Print();

  ASSERT_FALSE(chrome::IsCommandEnabled(browser(), IDC_PRINT));

  ASSERT_EQ(is_basic_print_expected,
            chrome::IsCommandEnabled(browser(), IDC_BASIC_PRINT));

  content::TestNavigationObserver reload_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::Reload(browser(), CURRENT_TAB);
  reload_observer.Wait();

  ASSERT_TRUE(chrome::IsCommandEnabled(browser(), IDC_PRINT));

  ASSERT_EQ(is_basic_print_expected,
            chrome::IsCommandEnabled(browser(), IDC_BASIC_PRINT));
}

// Disable the test for mac, see http://crbug/367665.
#if defined(OS_MACOSX) && !defined(OS_IOS)
#define MAYBE_TaskManagerNewPrintPreview DISABLED_TaskManagerNewPrintPreview
#else
#define MAYBE_TaskManagerNewPrintPreview TaskManagerNewPrintPreview
#endif
IN_PROC_BROWSER_TEST_F(PrintPreviewTest, MAYBE_TaskManagerNewPrintPreview) {
  chrome::ShowTaskManager(browser());  // Show task manager BEFORE print dialog.

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyPrint()));

  // Create the print preview dialog.
  Print();

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyPrint()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchPrint(url::kAboutBlankURL)));
}

// http://crbug/367665.
IN_PROC_BROWSER_TEST_F(PrintPreviewTest,
                       DISABLED_TaskManagerExistingPrintPreview) {
  // Create the print preview dialog.
  Print();

  chrome::ShowTaskManager(browser());  // Show task manager AFTER print dialog.

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyPrint()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchPrint(url::kAboutBlankURL)));
}

#if defined(OS_WIN)
// http://crbug.com/396360
IN_PROC_BROWSER_TEST_F(PrintPreviewTest, DISABLED_NoCrashOnCloseWithOtherTabs) {
  // Now print preview.
  Print();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  browser()->tab_strip_model()->ActivateTabAt(0, true);

  // Navigate main tab to hide print preview.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  browser()->tab_strip_model()->ActivateTabAt(1, true);
}
#endif  // defined(OS_WIN)

}  // namespace
