// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

class ChromeSitePerProcessTest : public InProcessBrowserTest {
 public:
  ChromeSitePerProcessTest() {}
  ~ChromeSitePerProcessTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    content::SetupCrossSiteRedirector(embedded_test_server());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSitePerProcessTest);
};

// Verify that browser shutdown path works correctly when there's a
// RenderFrameProxyHost for a child frame.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, RenderFrameProxyHostShutdown) {
  GURL main_url(embedded_test_server()->GetURL(
        "a.com",
        "/frame_tree/page_with_two_frames_remote_and_local.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);
}

// Verify that origin replication allows JS access to localStorage, database,
// and FileSystem APIs.  These features involve a check on the
// WebSecurityOrigin of the topmost WebFrame in ContentSettingsObserver, and
// this test ensures this check works when the top frame is remote.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest,
                       OriginReplicationAllowsAccessToStorage) {
  // Navigate to a page with a same-site iframe.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Navigate subframe cross-site.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL cross_site_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(NavigateIframeToURL(active_web_contents, "test", cross_site_url));

  // Find the subframe's RenderFrameHost.
  content::RenderFrameHost* frame_host =
      ChildFrameAt(active_web_contents->GetMainFrame(), 0);
  ASSERT_TRUE(frame_host);
  EXPECT_EQ(cross_site_url, frame_host->GetLastCommittedURL());
  EXPECT_TRUE(frame_host->IsCrossProcessSubframe());

  // Check that JS storage APIs can be accessed successfully.
  EXPECT_TRUE(
      content::ExecuteScript(frame_host, "localStorage['foo'] = 'bar'"));
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      frame_host, "window.domAutomationController.send(localStorage['foo']);",
      &result));
  EXPECT_EQ(result, "bar");
  bool is_object_created = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      frame_host,
      "window.domAutomationController.send(!!indexedDB.open('testdb', 2));",
      &is_object_created));
  EXPECT_TRUE(is_object_created);
  is_object_created = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      frame_host,
      "window.domAutomationController.send(!!openDatabase("
      "'foodb', '1.0', 'Test DB', 1024));",
      &is_object_created));
  EXPECT_TRUE(is_object_created);
  EXPECT_TRUE(ExecuteScript(frame_host,
                            "window.webkitRequestFileSystem("
                            "window.TEMPORARY, 1024, function() {});"));
}

// Ensure that creating a plugin in a cross-site subframe doesn't crash.  This
// involves querying content settings from the renderer process and using the
// top frame's origin as one of the parameters.  See https://crbug.com/426658.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, PluginWithRemoteTopFrame) {
  // Serve from the root so that flash_object.html can load the swf file.
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/chrome/test/data/iframe.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Navigate subframe to a page with a Flash object.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL frame_url =
      embedded_test_server()->GetURL("b.com",
                                     "/chrome/test/data/flash_object.html");

  // Ensure the page finishes loading without crashing.
  EXPECT_TRUE(NavigateIframeToURL(active_web_contents, "test", frame_url));
}

// Check that window.focus works for cross-process popups.
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessTest, PopupWindowFocus) {
  GURL main_url(embedded_test_server()->GetURL("/page_with_focus_events.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Set window.name on main page.  This will be used to identify the page
  // later when it sends messages from its focus/blur events.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(ExecuteScript(web_contents, "window.name = 'main'"));

  // Open a popup for a cross-site page.
  GURL popup_url =
      embedded_test_server()->GetURL("foo.com", "/page_with_focus_events.html");
  content::WindowedNotificationObserver popup_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  EXPECT_TRUE(ExecuteScript(web_contents,
                            "openPopup('" + popup_url.spec() + "','popup')"));
  popup_observer.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* popup =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(WaitForLoadStop(popup));
  EXPECT_EQ(popup_url, popup->GetLastCommittedURL());
  EXPECT_NE(popup, web_contents);

  // Switch focus to the original tab, since opening a popup also focused it.
  web_contents->GetDelegate()->ActivateContents(web_contents);
  EXPECT_EQ(web_contents, browser()->tab_strip_model()->GetActiveWebContents());

  // Focus the popup via window.focus().
  content::DOMMessageQueue queue;
  EXPECT_TRUE(ExecuteScript(web_contents, "focusPopup()"));

  // Wait for main page to lose focus and for popup to gain focus.  Each event
  // will send a message, and the two messages can arrive in any order.
  std::string status;
  bool main_lost_focus = false;
  bool popup_got_focus = false;
  while (queue.WaitForMessage(&status)) {
    if (status == "\"main-lost-focus\"")
      main_lost_focus = true;
    if (status == "\"popup-got-focus\"")
      popup_got_focus = true;
    if (main_lost_focus && popup_got_focus)
      break;
  }

  // The popup should be focused now.
  EXPECT_EQ(popup, browser()->tab_strip_model()->GetActiveWebContents());
}
