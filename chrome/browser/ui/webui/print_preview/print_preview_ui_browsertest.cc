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

  // Make sure advanced print command (Ctrl+Shift+p) is enabled.
  ASSERT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ADVANCED_PRINT));

  // Create the print preview dialog.
  Print();

  // Make sure print is disabled.
  ASSERT_FALSE(chrome::IsCommandEnabled(browser(), IDC_PRINT));

  // Make sure advanced print command (Ctrl+Shift+p) is enabled.
  ASSERT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ADVANCED_PRINT));

  content::TestNavigationObserver reload_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::Reload(browser(), CURRENT_TAB);
  reload_observer.Wait();

  ASSERT_TRUE(chrome::IsCommandEnabled(browser(), IDC_PRINT));

  // Make sure advanced print command (Ctrl+Shift+p) is enabled.
  ASSERT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ADVANCED_PRINT));
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

// Disable the test for mac as it started being flaky, see http://crbug/367665.
#if defined(OS_MACOSX) && !defined(OS_IOS)
#define MAYBE_TaskManagerExistingPrintPreview DISABLED_TaskManagerExistingPrintPreview
#else
#define MAYBE_TaskManagerExistingPrintPreview TaskManagerExistingPrintPreview
#endif
IN_PROC_BROWSER_TEST_F(PrintPreviewTest,
                       MAYBE_TaskManagerExistingPrintPreview) {
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
BOOL CALLBACK EnumerateChildren(HWND hwnd, LPARAM l_param) {
  HWND* child = reinterpret_cast<HWND*>(l_param);
  *child = hwnd;
  // The first child window is the plugin, then its children. So stop
  // enumerating after the first callback.
  return FALSE;
}

// This test verifies that constrained windows aren't covered by windowed NPAPI
// plugins. The code which fixes this is in WebContentsViewAura::WindowObserver.
IN_PROC_BROWSER_TEST_F(PrintPreviewTest, WindowedNPAPIPluginHidden) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPluginsAlwaysAuthorize,
                                               true);

  // First load the page and wait for the NPAPI plugin's window to display.
  base::string16 expected_title(base::ASCIIToUTF16("ready"));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher title_watcher(tab, expected_title);

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("printing"),
      base::FilePath().AppendASCII("npapi_plugin.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Now get the region of the plugin before and after the print preview is
  // shown. They should be different.
  HWND hwnd = tab->GetNativeView()->GetHost()->GetAcceleratedWidget();
  HWND child = NULL;
  EnumChildWindows(hwnd, EnumerateChildren,reinterpret_cast<LPARAM>(&child));

  RECT region_before, region_after;
  int result = GetWindowRgnBox(child, &region_before);
  ASSERT_EQ(result, SIMPLEREGION);

  // Now print preview.
  Print();

  result = GetWindowRgnBox(child, &region_after);
  if (result == NULLREGION) {
    // Depending on the browser window size, the plugin could be full covered.
    return;
  }

  if (result == COMPLEXREGION) {
    // Complex region, by definition not equal to the initial region.
    return;
  }

  ASSERT_EQ(result, SIMPLEREGION);
  bool rects_equal =
      region_before.left == region_after.left &&
      region_before.top == region_after.top &&
      region_before.right == region_after.right &&
      region_before.bottom == region_after.bottom;
  ASSERT_FALSE(rects_equal);
}

IN_PROC_BROWSER_TEST_F(PrintPreviewTest, NoCrashOnCloseWithOtherTabs) {
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
