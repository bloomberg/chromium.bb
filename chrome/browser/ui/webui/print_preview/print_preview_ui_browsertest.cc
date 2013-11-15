// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"

#if defined(OS_WIN) && defined(USE_AURA)
#include "content/public/browser/web_contents_view.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#endif

namespace {

class PrintPreviewTest : public InProcessBrowserTest {
 public:
  PrintPreviewTest() {}

#if !defined(GOOGLE_CHROME_BUILD)
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnablePrintPreview);
  }
#endif

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

#if defined(OS_WIN) && defined(USE_AURA)

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
  string16 expected_title(ASCIIToUTF16("ready"));
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
  HWND hwnd = tab->GetView()->GetNativeView()->GetDispatcher()->host()->
      GetAcceleratedWidget();
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
#endif

}  // namespace
