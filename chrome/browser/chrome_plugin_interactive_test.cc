// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_bar_host_unittest_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/process_type.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"

#if defined(OS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

#if defined(OS_WIN)

namespace {

BOOL CALLBACK EnumerateChildren(HWND hwnd, LPARAM l_param) {
  HWND* child = reinterpret_cast<HWND*>(l_param);
  *child = hwnd;
  // The first child window is the plugin, then its children. So stop
  // enumerating after the first callback.
  return FALSE;
}

}  // namespace

typedef InProcessBrowserTest ChromePluginTest;

// Test that if a background tab loads an NPAPI plugin, they are displayed after
// switching to that page.  http://crbug.com/335900
// flaky: http://crbug.com/406631
IN_PROC_BROWSER_TEST_F(ChromePluginTest, DISABLED_WindowedNPAPIPluginHidden) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPluginsAlwaysAuthorize,
                                               true);

  // First load the page in the background and wait for the NPAPI plugin's
  // window to be created.
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath().AppendASCII("windowed_npapi_plugin.html"));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // We create a third window just to trigger the second one to update its
  // constrained window list. Normally this would be triggered by the status bar
  // animation closing after the user middle clicked a link.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  base::string16 expected_title(base::ASCIIToUTF16("created"));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  if (tab->GetTitle() != expected_title) {
    content::TitleWatcher title_watcher(tab, expected_title);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  // Now activate the tab and verify that the plugin painted.
  browser()->tab_strip_model()->ActivateTabAt(1, true);

  base::string16 expected_title2(base::ASCIIToUTF16("shown"));
  content::TitleWatcher title_watcher2(tab, expected_title2);
  EXPECT_EQ(expected_title2, title_watcher2.WaitAndGetTitle());

  HWND child = NULL;
  HWND hwnd = tab->GetNativeView()->GetHost()->GetAcceleratedWidget();
  EnumChildWindows(hwnd, EnumerateChildren,reinterpret_cast<LPARAM>(&child));

  RECT region;
  int result = GetWindowRgnBox(child, &region);
  ASSERT_NE(result, NULLREGION);
}

typedef InProcessBrowserTest PrintPreviewTest;

// This test verifies that constrained windows aren't covered by windowed NPAPI
// plugins. The code which fixes this is in WebContentsViewAura::WindowObserver.
// flaky: http://crbug.com/406631
IN_PROC_BROWSER_TEST_F(PrintPreviewTest, DISABLED_WindowedNPAPIPluginHidden) {
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
  content::TestNavigationObserver nav_observer(NULL);
  nav_observer.StartWatchingNewWebContents();
  chrome::ExecuteCommand(browser(), IDC_PRINT);
  nav_observer.Wait();
  nav_observer.StopWatchingNewWebContents();

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

typedef InProcessBrowserTest FindInPageControllerTest;

void EnsureFindBoxOpen(Browser* browser) {
  chrome::ShowFindBar(browser);
  gfx::Point position;
  bool fully_visible = false;
  FindBarTesting* find_bar =
      browser->GetFindBarController()->find_bar()->GetFindBarTesting();
  EXPECT_TRUE(find_bar->GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);
}

// Ensure that the find bar is always over a windowed NPAPI plugin.
// flaky: http://crbug.com/406631
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       DISABLED_WindowedNPAPIPluginHidden) {
  chrome::DisableFindBarAnimationsDuringTesting(true);
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

  // Now get the region of the plugin before the find bar is shown.
  HWND hwnd = tab->GetNativeView()->GetHost()->GetAcceleratedWidget();
  HWND child = NULL;
  EnumChildWindows(hwnd, EnumerateChildren, reinterpret_cast<LPARAM>(&child));

  RECT region_before, region_after;
  int result = GetWindowRgnBox(child, &region_before);
  ASSERT_EQ(result, SIMPLEREGION);

  // Create a new tab and open the find bar there.
  chrome::NewTab(browser());
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  EnsureFindBoxOpen(browser());

  // Now switch back to the original tab with the plugin and show the find bar.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EnsureFindBoxOpen(browser());

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

#endif
