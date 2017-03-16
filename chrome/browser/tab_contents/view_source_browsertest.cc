// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/constants.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {
const char kTestHtml[] = "/viewsource/test.html";
const char kTestMedia[] = "/media/pink_noise_140ms.wav";
}

typedef InProcessBrowserTest ViewSourceTest;

// This test renders a page in view-source and then checks to see if the title
// set in the html was set successfully (it shouldn't because we rendered the
// page in view source).
// Flaky; see http://crbug.com/72201.
IN_PROC_BROWSER_TEST_F(ViewSourceTest, DoesBrowserRenderInViewSource) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // First we navigate to our view-source test page.
  GURL url(content::kViewSourceScheme + std::string(":") +
           embedded_test_server()->GetURL(kTestHtml).spec());
  ui_test_utils::NavigateToURL(browser(), url);

  // Check that the title didn't get set.  It should not be there (because we
  // are in view-source mode).
  EXPECT_NE(base::ASCIIToUTF16("foo"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

// This test renders a page normally and then renders the same page in
// view-source mode. This is done since we had a problem at one point during
// implementation of the view-source: prefix being consumed (removed from the
// URL) if the URL was not changed (apart from adding the view-source prefix)
IN_PROC_BROWSER_TEST_F(ViewSourceTest, DoesBrowserConsumeViewSourcePrefix) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // First we navigate to google.html.
  GURL url(embedded_test_server()->GetURL(kTestHtml));
  ui_test_utils::NavigateToURL(browser(), url);

  // Then we navigate to the same url but with the "view-source:" prefix.
  GURL url_viewsource(content::kViewSourceScheme + std::string(":") +
                      url.spec());
  ui_test_utils::NavigateToURL(browser(), url_viewsource);

  // The URL should still be prefixed with "view-source:".
  EXPECT_EQ(url_viewsource.spec(),
            browser()->tab_strip_model()->GetActiveWebContents()->
                GetURL().spec());
}

// Make sure that when looking at the actual page, we can select "View Source"
// from the menu.
IN_PROC_BROWSER_TEST_F(ViewSourceTest, ViewSourceInMenuEnabledOnANormalPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL(kTestHtml));
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_TRUE(chrome::CanViewSource(browser()));
}

// For page that is media content, make sure that we cannot select "View Source"
// See http://crbug.com/83714
IN_PROC_BROWSER_TEST_F(ViewSourceTest, ViewSourceInMenuDisabledOnAMediaPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL(kTestMedia));
  ui_test_utils::NavigateToURL(browser(), url);

  const char* mime_type = browser()->tab_strip_model()->GetActiveWebContents()->
      GetContentsMimeType().c_str();

  EXPECT_STREQ("audio/wav", mime_type);
  EXPECT_FALSE(chrome::CanViewSource(browser()));
}

// Make sure that when looking at the page source, we can't select "View Source"
// from the menu.
IN_PROC_BROWSER_TEST_F(ViewSourceTest,
                       ViewSourceInMenuDisabledWhileViewingSource) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_viewsource(content::kViewSourceScheme + std::string(":") +
                      embedded_test_server()->GetURL(kTestHtml).spec());
  ui_test_utils::NavigateToURL(browser(), url_viewsource);

  EXPECT_FALSE(chrome::CanViewSource(browser()));
}

// Tests that reload initiated by the script on the view-source page leaves
// the page in view-source mode.
// Times out on Mac, Windows, ChromeOS Linux: crbug.com/162080
IN_PROC_BROWSER_TEST_F(ViewSourceTest, DISABLED_TestViewSourceReload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_viewsource(content::kViewSourceScheme + std::string(":") +
                      embedded_test_server()->GetURL(kTestHtml).spec());

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), url_viewsource);
  observer.Wait();

  ASSERT_TRUE(
      content::ExecuteScript(browser()->tab_strip_model()->GetWebContentsAt(0),
                             "window.location.reload();"));

  content::WindowedNotificationObserver observer2(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  observer2.Wait();
  ASSERT_TRUE(browser()->tab_strip_model()->GetWebContentsAt(0)->
                  GetController().GetActiveEntry()->IsViewSourceMode());
}

// This test ensures that view-source session history navigations work
// correctly when switching processes. See https://crbug.com/544868.
IN_PROC_BROWSER_TEST_F(ViewSourceTest,
                       ViewSourceCrossProcessAndBack) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_viewsource(content::kViewSourceScheme + std::string(":") +
                      embedded_test_server()->GetURL(kTestHtml).spec());
  ui_test_utils::NavigateToURL(browser(), url_viewsource);
  EXPECT_FALSE(chrome::CanViewSource(browser()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Open another tab to the same origin, so the process is kept alive while
  // the original tab is navigated cross-process. This is required for the
  // original bug to reproduce.
  {
    GURL url = embedded_test_server()->GetURL("/title1.html");
    ui_test_utils::UrlLoadObserver load_complete(
        url, content::NotificationService::AllSources());
    EXPECT_TRUE(content::ExecuteScript(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.open('" + url.spec() + "');"));
    load_complete.Wait();
    EXPECT_EQ(2, browser()->tab_strip_model()->count());
  }

  // Switch back to the first tab and navigate it cross-process.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIVersionURL));
  EXPECT_TRUE(chrome::CanViewSource(browser()));

  // Navigate back in session history to ensure view-source mode is still
  // active.
  {
    ui_test_utils::UrlLoadObserver load_complete(
        url_viewsource, content::NotificationService::AllSources());
    chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
    load_complete.Wait();
  }

  // Check whether the page is in view-source mode or not by checking if an
  // expected element on the page exists or not. In view-source mode it
  // should not be found.
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "domAutomationController.send(document.getElementById('bar') === null);",
      &result));
  EXPECT_TRUE(result);
  EXPECT_FALSE(chrome::CanViewSource(browser()));
}

// Verify that restoring a view-source tab for a Chrome extension works
// properly.  See https://crbug.com/699428.
IN_PROC_BROWSER_TEST_F(ViewSourceTest, ViewSourceTabRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Go to the Chrome bookmarks URL.  It should redirect to the bookmark
  // manager Chrome extension.
  GURL bookmarks_url(chrome::kChromeUIBookmarksURL);
  ui_test_utils::NavigateToURL(browser(), bookmarks_url);
  EXPECT_TRUE(chrome::CanViewSource(browser()));
  content::WebContents* bookmarks_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL bookmarks_extension_url =
      bookmarks_tab->GetMainFrame()->GetLastCommittedURL();
  EXPECT_TRUE(bookmarks_extension_url.SchemeIs(extensions::kExtensionScheme));

  // Open a new view-source tab for that URL.
  GURL view_source_url(content::kViewSourceScheme + std::string(":") +
                       bookmarks_extension_url.spec());
  AddTabAtIndex(1, view_source_url, ui::PAGE_TRANSITION_TYPED);
  content::WebContents* view_source_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(view_source_url, view_source_tab->GetVisibleURL());
  EXPECT_EQ(view_source_url,
            view_source_tab->GetController().GetActiveEntry()->GetVirtualURL());
  EXPECT_EQ(bookmarks_extension_url,
            view_source_tab->GetMainFrame()->GetLastCommittedURL());
  EXPECT_FALSE(chrome::CanViewSource(browser()));

  // Close the view-source tab.
  chrome::CloseTab(browser());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Restore the tab.  In the bug, the restored navigation was blocked, and we
  // ended up showing view-source of an about:blank page.
  content::WindowedNotificationObserver tab_added_observer(
      chrome::NOTIFICATION_TAB_PARENTED,
      content::NotificationService::AllSources());
  chrome::RestoreTab(browser());
  tab_added_observer.Wait();
  view_source_tab = browser()->tab_strip_model()->GetActiveWebContents();
  WaitForLoadStop(view_source_tab);

  // Verify the browser-side URLs.  Note that without view-source, the
  // bookmarks extension visible URL would be rewritten to chrome://bookmarks,
  // but with view-source, we should still see it as
  // view-source:chrome-extension://.../.
  EXPECT_EQ(view_source_url, view_source_tab->GetVisibleURL());
  EXPECT_EQ(view_source_url,
            view_source_tab->GetController().GetActiveEntry()->GetVirtualURL());
  EXPECT_EQ(bookmarks_extension_url,
            view_source_tab->GetMainFrame()->GetLastCommittedURL());
  EXPECT_FALSE(chrome::CanViewSource(browser()));

  // Verify that the view-source content is not empty, and that the
  // renderer-side URL is correct.
  int view_source_length;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      view_source_tab,
      "domAutomationController.send(document.body.innerText.length)",
      &view_source_length));
  EXPECT_GT(view_source_length, 0);

  std::string location;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      view_source_tab, "domAutomationController.send(location.href)",
      &location));
  EXPECT_EQ(bookmarks_extension_url, location);
}
