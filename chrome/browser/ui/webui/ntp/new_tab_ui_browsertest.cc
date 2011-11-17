// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/gurl.h"

class NewTabUIBrowserTest : public InProcessBrowserTest {
 public:
  NewTabUIBrowserTest() {
    EnableDOMAutomation();
  }
};

// Ensure that chrome-internal: still loads the NTP.
// See http://crbug.com/6564.
IN_PROC_BROWSER_TEST_F(NewTabUIBrowserTest, ChromeInternalLoadsNTP) {
  // Go to the "new tab page" using its old url, rather than chrome://newtab.
  // Ensure that we get there by checking for non-empty page content.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome-internal:"));
  bool empty_inner_html = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetTabContentsAt(0)->render_view_host(), L"",
      L"window.domAutomationController.send(document.body.innerHTML == '')",
      &empty_inner_html));
  ASSERT_FALSE(empty_inner_html);
}

// Ensure loading a NTP with an existing SiteInstance in a reused process
// doesn't cause us to kill the process.  See http://crbug.com/104258.
IN_PROC_BROWSER_TEST_F(NewTabUIBrowserTest, LoadNTPInExistingProcess) {
  // Set max renderers to 1 to force running out of processes.
  RenderProcessHost::SetMaxRendererProcessCountForTest(1);

  // Start server for simple page.
  ASSERT_TRUE(test_server()->Start());

  // Load a NTP in a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(1,
            browser()->GetTabContentsAt(1)->GetSiteInstance()->max_page_id());

  // Navigate that tab to another site.  This allows the NTP process to exit,
  // but it keeps the NTP SiteInstance (and its max_page_id) alive in history.
  {
    // Wait not just for the navigation to finish, but for the NTP process to
    // exit as well.
    ui_test_utils::WindowedNotificationObserver process_exited_observer(
        content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
        content::NotificationService::AllSources());
    browser()->OpenURL(test_server()->GetURL("files/title1.html"), GURL(),
                       CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
    process_exited_observer.Wait();
  }

  // Create and close another two NTPs to inflate the max_page_id.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(2,
            browser()->GetTabContentsAt(2)->GetSiteInstance()->max_page_id());
  browser()->CloseTab();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(3,
            browser()->GetTabContentsAt(2)->GetSiteInstance()->max_page_id());
  browser()->CloseTab();

  // Open another Web UI page in a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(1,
            browser()->GetTabContentsAt(2)->GetSiteInstance()->max_page_id());

  // At this point, opening another NTP will use the old SiteInstance (with a
  // large max_page_id) in the existing Web UI process (with a small page ID).
  // Make sure we don't confuse this as an existing navigation to an unknown
  // entry.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(4,
            browser()->GetTabContentsAt(3)->GetSiteInstance()->max_page_id());
}
