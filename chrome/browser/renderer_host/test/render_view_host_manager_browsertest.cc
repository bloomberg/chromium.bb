// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/net_util.h"

class RenderViewHostManagerTest : public InProcessBrowserTest {
 public:
  RenderViewHostManagerTest() {
    EnableDOMAutomation();
  }
};

// Test for crbug.com/24447.  Following a cross-site link with rel=noreferrer
// and target=_blank should create a new SiteInstance.  Links with either
// rel=noreferrer or target=_blank (but not both) should not create a new
// SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       SwapProcessOnRelNoreferrerWithTargetBlank) {
  // Start two servers with different sites.
  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> http_server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  scoped_refptr<HTTPSTestServer> https_server =
      HTTPSTestServer::CreateGoodServer(kDocRoot);

  // Load a page with links that open in a new window.
  ui_test_utils::NavigateToURL(browser(), http_server->TestServerPageW(
      L"files/click-noreferrer-links.html"));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // 1. Test clicking a rel=noreferrer + target=blank link.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(clickNoRefTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);
  // Opens in new tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->selected_index());

  // Wait for the cross-site transition to finish.
  ui_test_utils::WaitForLoadStop(
      &(browser()->GetSelectedTabContents()->controller()));

  // Should have a new SiteInstance.
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noref_blank_site_instance);

  // Close the tab to try another link.
  browser()->CloseTab();

  // 2. Test clicking a target=blank link.
  success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(clickTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);
  // Opens in new tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->selected_index());

  // Wait for the cross-site transition to finish.
  ui_test_utils::WaitForLoadStop(
      &(browser()->GetSelectedTabContents()->controller()));

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Close the tab to try another link.
  browser()->CloseTab();

  // 3. Test clicking a rel=noreferrer link.
  success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(clickNoRefLink());",
      &success));
  EXPECT_TRUE(success);
  // Opens in same tab.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(0, browser()->selected_index());

  // Wait for the cross-site transition to finish.
  ui_test_utils::WaitForLoadStop(
      &(browser()->GetSelectedTabContents()->controller()));

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> noref_site_instance(
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, noref_site_instance);
}

// Test for crbug.com/14505. This tests that chrome:// urls are still functional
// after download of a file while viewing another chrome://.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       DISABLED_ChromeURLAfterDownload) {
  GURL downloads_url("chrome://downloads");
  GURL extensions_url("chrome://extensions");
  FilePath zip_download;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &zip_download));
  zip_download = zip_download.AppendASCII("zip").AppendASCII("test.zip");
  GURL zip_url = net::FilePathToFileURL(zip_download);

  ui_test_utils::NavigateToURL(browser(), downloads_url);
  ui_test_utils::NavigateToURL(browser(), zip_url);
  ui_test_utils::WaitForDownloadCount(
      browser()->profile()->GetDownloadManager(), 1);
  ui_test_utils::NavigateToURL(browser(), extensions_url);

  TabContents *contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(contents);
  bool domui_responded = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents->render_view_host(),
      L"",
      L"window.domAutomationController.send(window.domui_responded_);",
      &domui_responded));
  EXPECT_TRUE(domui_responded);
}

class BrowserClosedObserver : public NotificationObserver {
 public:
  explicit BrowserClosedObserver(Browser* browser) {
    registrar_.Add(this, NotificationType::BROWSER_CLOSED,
        Source<Browser>(browser));
    ui_test_utils::RunMessageLoop();
  }

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::BROWSER_CLOSED:
        MessageLoopForUI::current()->Quit();
        break;
    }
  }

 private:
  NotificationRegistrar registrar_;
};

// Test for crbug.com/12745. This tests that if a download is initiated from
// a chrome:// page that has registered and onunload handler, the browser
// will be able to close.
// TODO(rafaelw): The fix for 12745 has now also been reverted. Another fix
// must be found before this can be re-enabled.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       DISABLED_BrowserCloseAfterDownload) {
  GURL downloads_url("chrome://downloads");
  FilePath zip_download;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &zip_download));
  zip_download = zip_download.AppendASCII("zip").AppendASCII("test.zip");
  ASSERT_TRUE(file_util::PathExists(zip_download));
  GURL zip_url = net::FilePathToFileURL(zip_download);

  ui_test_utils::NavigateToURL(browser(), downloads_url);
  TabContents *contents = browser()->GetSelectedTabContents();
  ASSERT_TRUE(contents);
  bool result = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents->render_view_host(),
      L"",
      L"window.onunload = function() { var do_nothing = 0; }; "
      L"window.domAutomationController.send(true);",
      &result));
  EXPECT_TRUE(result);
  ui_test_utils::NavigateToURL(browser(), zip_url);

  ui_test_utils::WaitForDownloadCount(
    browser()->profile()->GetDownloadManager(), 1);

  browser()->CloseWindow();
  BrowserClosedObserver wait_for_close(browser());
}
