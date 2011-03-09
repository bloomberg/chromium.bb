// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/ref_counted.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_type.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"

class RenderViewHostManagerTest : public InProcessBrowserTest {
 public:
  RenderViewHostManagerTest() {
    EnableDOMAutomation();
  }

  static bool GetFilePathWithHostAndPortReplacement(
      const std::string& original_file_path,
      const net::HostPortPair& host_port_pair,
      std::string* replacement_path) {
    std::vector<net::TestServer::StringPair> replacement_text;
    replacement_text.push_back(
        make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
    return net::TestServer::GetFilePathWithReplacements(
        original_file_path, replacement_text, replacement_path);
  }
};

// Test for crbug.com/24447.  Following a cross-site link with rel=noreferrer
// and target=_blank should create a new SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       SwapProcessWithRelNoreferrerAndTargetBlank) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server_(
      net::TestServer::TYPE_HTTPS,
      FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server_.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server_.host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a rel=noreferrer + target=blank link.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(clickNoRefTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);
  // Wait for the cross-site transition to finish.
  ui_test_utils::WaitForLoadStop(
      &(browser()->GetSelectedTabContents()->controller()));

  // Opens in new tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->selected_index());
  EXPECT_EQ(L"Title Of Awesomeness",
      browser()->GetSelectedTabContents()->GetTitle());

  // Should have a new SiteInstance.
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noref_blank_site_instance);
}

// Test for crbug.com/24447.  Following a cross-site link with just
// target=_blank should not create a new SiteInstance.
// Disabled, http://crbug.com/67532.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       DISABLED_DontSwapProcessWithOnlyTargetBlank) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server_(
      net::TestServer::TYPE_HTTPS,
      FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server_.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server_.host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a target=blank link.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(clickTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);
  // Wait for the cross-site transition to finish.
  ui_test_utils::WaitForLoadStop(
      &(browser()->GetSelectedTabContents()->controller()));

  // Opens in new tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->selected_index());
  EXPECT_EQ(L"Title Of Awesomeness",
      browser()->GetSelectedTabContents()->GetTitle());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);
}

// Test for crbug.com/24447.  Following a cross-site link with rel=noreferrer
// and no target=_blank should not create a new SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       DontSwapProcessWithOnlyRelNoreferrer) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server_(
      net::TestServer::TYPE_HTTPS,
      FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server_.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server_.host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a rel=noreferrer link.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(clickNoRefLink());",
      &success));
  EXPECT_TRUE(success);
  // Wait for the cross-site transition to finish.
  ui_test_utils::WaitForLoadStop(
      &(browser()->GetSelectedTabContents()->controller()));

  // Opens in same tab.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(0, browser()->selected_index());
  EXPECT_EQ(L"Title Of Awesomeness",
      browser()->GetSelectedTabContents()->GetTitle());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> noref_site_instance(
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, noref_site_instance);
}

// Hangs flakily in Win, http://crbug.com/45040.
#if defined(OS_WIN)
#define MAYBE_ChromeURLAfterDownload DISABLED_ChromeURLAfterDownload
#else
#define MAYBE_ChromeURLAfterDownload ChromeURLAfterDownload
#endif  // defined(OS_WIN)

// Test for crbug.com/14505. This tests that chrome:// urls are still functional
// after download of a file while viewing another chrome://.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       MAYBE_ChromeURLAfterDownload) {
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
  bool webui_responded = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents->render_view_host(),
      L"",
      L"window.domAutomationController.send(window.webui_responded_);",
      &webui_responded));
  EXPECT_TRUE(webui_responded);
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
