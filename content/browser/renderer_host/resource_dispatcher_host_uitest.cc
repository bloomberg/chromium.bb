// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/net/url_request_failed_dns_job.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"

namespace {

class ResourceDispatcherTest : public UITest {
 public:
  void CheckTitleTest(const std::string& file,
                      const std::string& expected_title,
                      int expected_navigations) {
    NavigateToURLBlockUntilNavigationsComplete(
        URLRequestMockHTTPJob::GetMockUrl(FilePath().AppendASCII(file)),
        expected_navigations);
    EXPECT_EQ(expected_title, WideToASCII(GetActiveTabTitle()));
  }

 protected:
  ResourceDispatcherTest() : UITest() {
    dom_automation_enabled_ = true;
  }
};

TEST_F(ResourceDispatcherTest, SniffHTMLWithNoContentType) {
  CheckTitleTest("content-sniffer-test0.html",
                 "Content Sniffer Test 0", 1);
}

TEST_F(ResourceDispatcherTest, RespectNoSniffDirective) {
  CheckTitleTest("nosniff-test.html", "mock.http/nosniff-test.html", 1);
}

TEST_F(ResourceDispatcherTest, DoNotSniffHTMLFromTextPlain) {
  CheckTitleTest("content-sniffer-test1.html",
                 "mock.http/content-sniffer-test1.html", 1);
}

TEST_F(ResourceDispatcherTest, DoNotSniffHTMLFromImageGIF) {
  CheckTitleTest("content-sniffer-test2.html",
                 "mock.http/content-sniffer-test2.html", 1);
}

TEST_F(ResourceDispatcherTest, SniffNoContentTypeNoData) {
  CheckTitleTest("content-sniffer-test3.html",
                 "Content Sniffer Test 3", 1);
  EXPECT_EQ(1, GetTabCount());

  // Make sure the download shelf is not showing.
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  bool visible = false;
  ASSERT_TRUE(browser->IsShelfVisible(&visible));
  EXPECT_FALSE(visible);
}

TEST_F(ResourceDispatcherTest, ContentDispositionEmpty) {
  CheckTitleTest("content-disposition-empty.html", "success", 1);
}

TEST_F(ResourceDispatcherTest, ContentDispositionInline) {
  CheckTitleTest("content-disposition-inline.html", "success", 1);
}

// Test for bug #1091358.
// Flaky: http://crbug.com/62595
TEST_F(ResourceDispatcherTest, FLAKY_SyncXMLHttpRequest) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(test_server.Start());

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab(browser_proxy->GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(test_server.GetURL(
                "files/sync_xmlhttprequest.html")));

  // Let's check the XMLHttpRequest ran successfully.
  bool success = false;
  EXPECT_TRUE(tab->ExecuteAndExtractBool(L"",
      L"window.domAutomationController.send(DidSyncRequestSucceed());",
      &success));
  EXPECT_TRUE(success);
}

// http://code.google.com/p/chromium/issues/detail?id=62776
TEST_F(ResourceDispatcherTest, FLAKY_SyncXMLHttpRequest_Disallowed) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(test_server.Start());

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab(browser_proxy->GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(test_server.GetURL(
                "files/sync_xmlhttprequest_disallowed.html")));

  // Let's check the XMLHttpRequest ran successfully.
  bool success = false;
  EXPECT_TRUE(tab->ExecuteAndExtractBool(L"",
      L"window.domAutomationController.send(DidSucceed());",
      &success));
  EXPECT_TRUE(success);
}

// Test for bug #1159553 -- A synchronous xhr (whose content-type is
// downloadable) would trigger download and hang the renderer process,
// if executed while navigating to a new page.
// Disabled -- http://code.google.com/p/chromium/issues/detail?id=56264
TEST_F(ResourceDispatcherTest, DISABLED_SyncXMLHttpRequest_DuringUnload) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(test_server.Start());

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab(browser_proxy->GetActiveTab());
  ASSERT_TRUE(tab.get());

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(test_server.GetURL(
                "files/sync_xmlhttprequest_during_unload.html")));

  // Confirm that the page has loaded (since it changes its title during load).
  std::wstring tab_title;
  EXPECT_TRUE(tab->GetTabTitle(&tab_title));
  EXPECT_EQ(L"sync xhr on unload", tab_title);

  // Navigate to a new page, to dispatch unload event and trigger xhr.
  // (the bug would make this step hang the renderer).
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(test_server.GetURL("files/title2.html")));

  // Check that the new page got loaded, and that no download was triggered.
  EXPECT_TRUE(tab->GetTabTitle(&tab_title));
  EXPECT_EQ(L"Title Of Awesomeness", tab_title);

  bool shelf_is_visible = false;
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  EXPECT_TRUE(browser->IsShelfVisible(&shelf_is_visible));
  EXPECT_FALSE(shelf_is_visible);
}

// Tests that onunload is run for cross-site requests.  (Bug 1114994)
TEST_F(ResourceDispatcherTest, CrossSiteOnunloadCookie) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(test_server.Start());

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab(browser_proxy->GetActiveTab());
  ASSERT_TRUE(tab.get());

  GURL url(test_server.GetURL("files/onunload_cookie.html"));
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(url));

  // Confirm that the page has loaded (since it changes its title during load).
  std::wstring tab_title;
  EXPECT_TRUE(tab->GetTabTitle(&tab_title));
  EXPECT_EQ(L"set cookie on unload", tab_title);

  // Navigate to a new cross-site page, to dispatch unload event and set the
  // cookie.
  CheckTitleTest("content-sniffer-test0.html",
                 "Content Sniffer Test 0", 1);

  // Check that the cookie was set.
  std::string value_result;
  ASSERT_TRUE(tab->GetCookieByName(url, "onunloadCookie", &value_result));
  ASSERT_FALSE(value_result.empty());
  ASSERT_STREQ("foo", value_result.c_str());
}

#if !defined(OS_MACOSX)
// Tests that the onbeforeunload and onunload logic is shortcutted if the old
// renderer is gone.  In that case, we don't want to wait for the old renderer
// to run the handlers.
// We need to disable this on Mac because the crash causes the OS CrashReporter
// process to kick in to analyze the poor dead renderer.  Unfortunately, if the
// app isn't stripped of debug symbols, this takes about five minutes to
// complete and isn't conducive to quick turnarounds. As we don't currently
// strip the app on the build bots, this is bad times.
TEST_F(ResourceDispatcherTest, CrossSiteAfterCrash) {
  // This test only works in multi-process mode
  if (ProxyLauncher::in_process_renderer())
    return;

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab(browser_proxy->GetActiveTab());
  ASSERT_TRUE(tab.get());

  // Cause the renderer to crash.
#if defined(OS_WIN) || defined(USE_LINUX_BREAKPAD)
  expected_crashes_ = 1;
#endif
  ASSERT_TRUE(tab->NavigateToURLAsync(GURL(chrome::kAboutCrashURL)));
  // Wait for browser to notice the renderer crash.
  base::PlatformThread::Sleep(TestTimeouts::action_timeout_ms());

  // Navigate to a new cross-site page.  The browser should not wait around for
  // the old renderer's on{before}unload handlers to run.
  CheckTitleTest("content-sniffer-test0.html",
                 "Content Sniffer Test 0", 1);
}
#endif  // !defined(OS_MACOSX)

// Tests that cross-site navigations work when the new page does not go through
// the BufferedEventHandler (e.g., non-http{s} URLs).  (Bug 1225872)
TEST_F(ResourceDispatcherTest, CrossSiteNavigationNonBuffered) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab(browser_proxy->GetActiveTab());
  ASSERT_TRUE(tab.get());

  // Start with an HTTP page.
  CheckTitleTest("content-sniffer-test0.html",
                 "Content Sniffer Test 0", 1);

  // Now load a file:// page, which does not use the BufferedEventHandler.
  // Make sure that the page loads and displays a title, and doesn't get stuck.
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("title2.html");
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(net::FilePathToFileURL(test_file)));
  EXPECT_EQ(L"Title Of Awesomeness", GetActiveTabTitle());
}

// Tests that a cross-site navigation to an error page (resulting in the link
// doctor page) still runs the onunload handler and can support navigations
// away from the link doctor page.  (Bug 1235537)
TEST_F(ResourceDispatcherTest, CrossSiteNavigationErrorPage) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(test_server.Start());

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab(browser_proxy->GetActiveTab());
  ASSERT_TRUE(tab.get());

  GURL url(test_server.GetURL("files/onunload_cookie.html"));
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(url));

  // Confirm that the page has loaded (since it changes its title during load).
  std::wstring tab_title;
  EXPECT_TRUE(tab->GetTabTitle(&tab_title));
  EXPECT_EQ(L"set cookie on unload", tab_title);

  // Navigate to a new cross-site URL that results in an error page.
  // TODO(creis): If this causes crashes or hangs, it might be for the same
  // reason as ErrorPageTest::DNSError.  See bug 1199491 and
  // http://crbug.com/22877.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURLBlockUntilNavigationsComplete(
                GURL(URLRequestFailedDnsJob::kTestUrl), 2));
  EXPECT_NE(L"set cookie on unload", GetActiveTabTitle());

  // Check that the cookie was set, meaning that the onunload handler ran.
  std::string value_result;
  EXPECT_TRUE(tab->GetCookieByName(url, "onunloadCookie", &value_result));
  EXPECT_FALSE(value_result.empty());
  EXPECT_STREQ("foo", value_result.c_str());

  // Check that renderer-initiated navigations still work.  In a previous bug,
  // the ResourceDispatcherHost would think that such navigations were
  // cross-site, because we didn't clean up from the previous request.  Since
  // TabContents was in the NORMAL state, it would ignore the attempt to run
  // the onunload handler, and the navigation would fail.
  // (Test by redirecting to javascript:window.location='someURL'.)
  GURL test_url(test_server.GetURL("files/title2.html"));
  std::string redirect_url = "javascript:window.location='" +
      test_url.possibly_invalid_spec() + "'";
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(GURL(redirect_url)));
  EXPECT_TRUE(tab->GetTabTitle(&tab_title));
  EXPECT_EQ(L"Title Of Awesomeness", tab_title);
}

TEST_F(ResourceDispatcherTest, CrossOriginRedirectBlocked) {
  // We expect the following URL requests from this test:
  // 1-  http://mock.http/cross-origin-redirect-blocked.html
  // 2-  http://mock.http/redirect-to-title2.html
  // 3-  http://mock.http/title2.html
  //
  // If the redirect in #2 were not blocked, we'd also see a request
  // for http://mock.http:4000/title2.html, and the title would be different.
  CheckTitleTest("cross-origin-redirect-blocked.html",
                 "Title Of More Awesomeness", 2);
}

// Tests that ResourceDispatcherHostRequestInfo is updated correctly on failed
// requests, to prevent calling Read on a request that has already failed.
// See bug 40250.
TEST_F(ResourceDispatcherTest, CrossSiteFailedRequest) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab(browser_proxy->GetActiveTab());
  ASSERT_TRUE(tab.get());

  // Visit another URL first to trigger a cross-site navigation.
  GURL url(chrome::kChromeUINewTabURL);
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(url));

  // Visit a URL that fails without calling ResourceDispatcherHost::Read.
  GURL broken_url("chrome://theme");
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(broken_url));

  // Make sure the navigation finishes.
  std::wstring tab_title;
  EXPECT_TRUE(tab->GetTabTitle(&tab_title));
  EXPECT_EQ(L"chrome://theme/ is not available", tab_title);
}

}  // namespace
