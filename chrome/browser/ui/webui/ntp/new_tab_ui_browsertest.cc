// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "googleurl/src/gurl.h"

using content::OpenURLParams;
using content::Referrer;

class NewTabUIBrowserTest : public InProcessBrowserTest {
 public:
  NewTabUIBrowserTest() {}
};

// Ensure that chrome-internal: still loads the NTP.
// See http://crbug.com/6564.
IN_PROC_BROWSER_TEST_F(NewTabUIBrowserTest, ChromeInternalLoadsNTP) {
  // Go to the "new tab page" using its old url, rather than chrome://newtab.
  // Ensure that we get there by checking for non-empty page content.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome-internal:"));
  bool empty_inner_html = false;
  ASSERT_TRUE(content::ExecuteJavaScriptAndExtractBool(
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(document.body.innerHTML == '')",
      &empty_inner_html));
  ASSERT_FALSE(empty_inner_html);
}

// Ensure loading a NTP with an existing SiteInstance in a reused process
// doesn't cause us to kill the process.  See http://crbug.com/104258.
IN_PROC_BROWSER_TEST_F(NewTabUIBrowserTest, LoadNTPInExistingProcess) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  // Start server for simple page.
  ASSERT_TRUE(test_server()->Start());

  // Load a NTP in a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(1,
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetMaxPageID());

  // Navigate that tab to another site.  This allows the NTP process to exit,
  // but it keeps the NTP SiteInstance (and its max_page_id) alive in history.
  {
    // Wait not just for the navigation to finish, but for the NTP process to
    // exit as well.
    content::WindowedNotificationObserver process_exited_observer(
        content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
        content::NotificationService::AllSources());
    browser()->OpenURL(OpenURLParams(
        test_server()->GetURL("files/title1.html"), Referrer(), CURRENT_TAB,
        content::PAGE_TRANSITION_TYPED, false));
    process_exited_observer.Wait();
  }

  // Creating a NTP in another tab should not be affected, since page IDs
  // are now specific to a tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(1,
            browser()->tab_strip_model()->GetWebContentsAt(2)->GetMaxPageID());
  chrome::CloseTab(browser());

  // Open another Web UI page in a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(1,
            browser()->tab_strip_model()->GetWebContentsAt(2)->GetMaxPageID());

  // At this point, opening another NTP will use the existing WebUI process
  // but its own SiteInstance, so the page IDs shouldn't affect each other.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(1,
            browser()->tab_strip_model()->GetWebContentsAt(3)->GetMaxPageID());

  // Navigating to the NTP in the original tab causes a BrowsingInstance
  // swap, so it gets a new SiteInstance starting with page ID 1 again.
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(1,
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetMaxPageID());
}

// Loads chrome://hang/ into two NTP tabs, ensuring we don't crash.
// See http://crbug.com/59859.
// If this flakes, use http://crbug.com/87200.
IN_PROC_BROWSER_TEST_F(NewTabUIBrowserTest, ChromeHangInNTP) {
  // Bring up a new tab page.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Navigate to chrome://hang/ to stall the process.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIHangURL), CURRENT_TAB, 0);

  // Visit chrome://hang/ again in another NTP. Don't bother waiting for the
  // NTP to load, because it's hung.
  chrome::NewTab(browser());
  browser()->OpenURL(OpenURLParams(
      GURL(chrome::kChromeUIHangURL), Referrer(), CURRENT_TAB,
      content::PAGE_TRANSITION_TYPED, false));
}

class NewTabUIProcessPerTabTest : public NewTabUIBrowserTest {
 public:
   NewTabUIProcessPerTabTest() {}

   virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
     command_line->AppendSwitch(switches::kProcessPerTab);
   }
};

// Navigates away from NTP before it commits, in process-per-tab mode.
// Ensures that we don't load the normal page in the NTP process (and thus
// crash), as in http://crbug.com/69224.
// If this flakes, use http://crbug.com/87200
IN_PROC_BROWSER_TEST_F(NewTabUIProcessPerTabTest, NavBeforeNTPCommits) {
  // Bring up a new tab page.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  // Navigate to chrome://hang/ to stall the process.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIHangURL), CURRENT_TAB, 0);

  // Visit a normal URL in another NTP that hasn't committed.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB, 0);

  // We don't use ui_test_utils::NavigateToURLWithDisposition because that waits
  // for current loading to stop.
  content::TestNavigationObserver observer(
      content::NotificationService::AllSources());
  browser()->OpenURL(OpenURLParams(
      GURL("data:text/html,hello world"), Referrer(), CURRENT_TAB,
      content::PAGE_TRANSITION_TYPED, false));
  observer.Wait();
}
