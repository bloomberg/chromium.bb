// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Navigates the browser to server and client redirect pages and makes sure
// that the correct redirects are reflected in the history database. Errors
// here might indicate that WebKit changed the calls our glue layer gets in
// the case of redirects. It may also mean problems with the history system.

#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/string16.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"
#include "ui/base/events.h"

namespace {

class RedirectTest : public UITest {
 public:
  RedirectTest()
      : test_server_(net::TestServer::TYPE_HTTP,
                     FilePath(FILE_PATH_LITERAL("chrome/test/data"))) {
  }

 protected:
  net::TestServer test_server_;
};

// Tests a single server redirect
TEST_F(RedirectTest, Server) {
  ASSERT_TRUE(test_server_.Start());

  GURL final_url = test_server_.GetURL(std::string());
  GURL first_url = test_server_.GetURL(
      "server-redirect?" + final_url.spec());

  NavigateToURL(first_url);

  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());

  std::vector<GURL> redirects;
  ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));

  ASSERT_EQ(1U, redirects.size());
  EXPECT_EQ(final_url.spec(), redirects[0].spec());
}

// Tests a single client redirect.
TEST_F(RedirectTest, Client) {
  ASSERT_TRUE(test_server_.Start());

  GURL final_url = test_server_.GetURL(std::string());
  GURL first_url = test_server_.GetURL(
      "client-redirect?" + final_url.spec());

  // The client redirect appears as two page visits in the browser.
  NavigateToURLBlockUntilNavigationsComplete(first_url, 2);

  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());

  std::vector<GURL> redirects;
  ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));

  ASSERT_EQ(1U, redirects.size());
  EXPECT_EQ(final_url.spec(), redirects[0].spec());

  // The address bar should display the final URL.
  GURL tab_url;
  EXPECT_TRUE(tab_proxy->GetCurrentURL(&tab_url));
  EXPECT_TRUE(final_url == tab_url);

  // Navigate one more time.
  NavigateToURLBlockUntilNavigationsComplete(first_url, 2);

  // The address bar should still display the final URL.
  EXPECT_TRUE(tab_proxy->GetCurrentURL(&tab_url));
  EXPECT_TRUE(final_url == tab_url);
}

// http://code.google.com/p/chromium/issues/detail?id=62772
TEST_F(RedirectTest, FLAKY_ClientEmptyReferer) {
  ASSERT_TRUE(test_server_.Start());

  // Create the file contents, which will do a redirect to the
  // test server.
  GURL final_url = test_server_.GetURL(std::string());
  ASSERT_TRUE(final_url.is_valid());
  std::string file_redirect_contents = StringPrintf(
      "<html>"
      "<head></head>"
      "<body onload=\"document.location='%s'\"></body>"
      "</html>",
      final_url.spec().c_str());

  // Write the contents to a temporary file.
  ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  FilePath temp_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_directory.path(),
                                                  &temp_file));
  ASSERT_EQ(static_cast<int>(file_redirect_contents.size()),
            file_util::WriteFile(temp_file,
                                 file_redirect_contents.data(),
                                 file_redirect_contents.size()));

  // Navigate to the file through the browser. The client redirect will appear
  // as two page visits in the browser.
  GURL first_url = net::FilePathToFileURL(temp_file);
  NavigateToURLBlockUntilNavigationsComplete(first_url, 2);

  std::vector<GURL> redirects;
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));
  ASSERT_EQ(1U, redirects.size());
  EXPECT_EQ(final_url.spec(), redirects[0].spec());
}

// Tests to make sure a location change when a pending redirect exists isn't
// flagged as a redirect.
#if defined(OS_MACOSX)
// SimulateOSClick is broken on the Mac: http://crbug.com/45162
#define MAYBE_ClientCancelled DISABLED_ClientCancelled
#elif defined(OS_WIN)
// http://crbug.com/53091
#define MAYBE_ClientCancelled FAILS_ClientCancelled
#else
#define MAYBE_ClientCancelled ClientCancelled
#endif

TEST_F(RedirectTest, MAYBE_ClientCancelled) {
  FilePath first_path(test_data_directory_);
  first_path = first_path.AppendASCII("cancelled_redirect_test.html");
  ASSERT_TRUE(file_util::AbsolutePath(&first_path));
  GURL first_url = net::FilePathToFileURL(first_path);

  NavigateToURLBlockUntilNavigationsComplete(first_url, 1);

  scoped_refptr<BrowserProxy> browser = automation()->GetBrowserWindow(0);
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window = browser->GetWindow();
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  int64 last_nav_time = 0;
  EXPECT_TRUE(tab_proxy->GetLastNavigationTime(&last_nav_time));
  // Simulate a click to force to make a user-initiated location change;
  // otherwise, a non user-initiated in-page location change will be treated
  // as client redirect and the redirect will be recoreded, which can cause
  // this test failed.
  gfx::Rect tab_view_bounds;
  ASSERT_TRUE(browser->BringToFront());
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_CONTAINER, &tab_view_bounds,
                                    true));
  ASSERT_TRUE(window->SimulateOSClick(tab_view_bounds.CenterPoint(),
                                      ui::EF_LEFT_BUTTON_DOWN));
  EXPECT_TRUE(tab_proxy->WaitForNavigation(last_nav_time));

  std::vector<GURL> redirects;
  ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));

  // There should be no redirects from first_url, because the anchor location
  // change that occurs should not be flagged as a redirect and the meta-refresh
  // won't have fired yet.
  ASSERT_EQ(0U, redirects.size());
  GURL current_url;
  ASSERT_TRUE(tab_proxy->GetCurrentURL(&current_url));

  // Need to test final path and ref separately since constructing a file url
  // containing an anchor using FilePathToFileURL will escape the anchor as
  // %23, but in current_url the anchor will be '#'.
  std::string final_ref = "myanchor";
  FilePath current_path;
  ASSERT_TRUE(net::FileURLToFilePath(current_url, &current_path));
  ASSERT_TRUE(file_util::AbsolutePath(&current_path));
  // Path should remain unchanged.
  EXPECT_EQ(StringToLowerASCII(first_path.value()),
            StringToLowerASCII(current_path.value()));
  EXPECT_EQ(final_ref, current_url.ref());
}

// Tests a client->server->server redirect
TEST_F(RedirectTest, ClientServerServer) {
  ASSERT_TRUE(test_server_.Start());

  GURL final_url = test_server_.GetURL(std::string());
  GURL next_to_last = test_server_.GetURL(
      "server-redirect?" + final_url.spec());
  GURL second_url = test_server_.GetURL(
      "server-redirect?" + next_to_last.spec());
  GURL first_url = test_server_.GetURL(
      "client-redirect?" + second_url.spec());
  std::vector<GURL> redirects;

  // We need the sleep for the client redirects, because it appears as two
  // page visits in the browser.
  NavigateToURL(first_url);

  for (int i = 0; i < 10; ++i) {
    base::PlatformThread::Sleep(TestTimeouts::action_timeout_ms());
    scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
    ASSERT_TRUE(tab_proxy.get());
    ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));
    if (!redirects.empty())
      break;
  }

  ASSERT_EQ(3U, redirects.size());
  EXPECT_EQ(second_url.spec(), redirects[0].spec());
  EXPECT_EQ(next_to_last.spec(), redirects[1].spec());
  EXPECT_EQ(final_url.spec(), redirects[2].spec());
}

// Tests that the "#reference" gets preserved across server redirects.
TEST_F(RedirectTest, ServerReference) {
  ASSERT_TRUE(test_server_.Start());

  const std::string ref("reference");

  GURL final_url = test_server_.GetURL(std::string());
  GURL initial_url = test_server_.GetURL(
      "server-redirect?" + final_url.spec() + "#" + ref);

  NavigateToURL(initial_url);

  GURL url = GetActiveTabURL();
  EXPECT_EQ(ref, url.ref());
}

// Test that redirect from http:// to file:// :
// A) does not crash the browser or confuse the redirect chain, see bug 1080873
// B) does not take place.
//
// Flaky on XP and Vista, http://crbug.com/69390.
TEST_F(RedirectTest, FLAKY_NoHttpToFile) {
  ASSERT_TRUE(test_server_.Start());
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("http_to_file.html");
  GURL file_url = net::FilePathToFileURL(test_file);

  GURL initial_url = test_server_.GetURL(
      "client-redirect?" + file_url.spec());

  NavigateToURL(initial_url);
  // UITest will check for crashes. We make sure the title doesn't match the
  // title from the file, because the nav should not have taken place.
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  std::wstring actual_title;
  ASSERT_TRUE(tab_proxy->GetTabTitle(&actual_title));
  EXPECT_NE("File!", WideToUTF8(actual_title));
}

// Ensures that non-user initiated location changes (within page) are
// flagged as client redirects. See bug 1139823.
TEST_F(RedirectTest, ClientFragments) {
  ASSERT_TRUE(test_server_.Start());

  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("ref_redirect.html");
  GURL first_url = net::FilePathToFileURL(test_file);
  std::vector<GURL> redirects;

  NavigateToURL(first_url);

  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));
  EXPECT_EQ(1U, redirects.size());
  EXPECT_EQ(first_url.spec() + "#myanchor", redirects[0].spec());
}

// TODO(timsteele): This is disabled because our current testserver can't
// handle multiple requests in parallel, making it hang on the first request
// to /slow?60. It's unable to serve our second request for files/title2.html
// until /slow? completes, which doesn't give the desired behavior. We could
// alternatively load the second page from disk, but we would need to start
// the browser for this testcase with --process-per-tab, and I don't think
// we can do this at test-case-level granularity at the moment.
// http://crbug.com/45056
TEST_F(RedirectTest,
       DISABLED_ClientCancelledByNewNavigationAfterProvisionalLoad) {
  // We want to initiate a second navigation after the provisional load for
  // the client redirect destination has started, but before this load is
  // committed. To achieve this, we tell the browser to load a slow page,
  // which causes it to start a provisional load, and while it is waiting
  // for the response (which means it hasn't committed the load for the client
  // redirect destination page yet), we issue a new navigation request.
  ASSERT_TRUE(test_server_.Start());

  GURL final_url = test_server_.GetURL("files/title2.html");
  GURL slow = test_server_.GetURL("slow?60");
  GURL first_url = test_server_.GetURL(
      "client-redirect?" + slow.spec());
  std::vector<GURL> redirects;

  NavigateToURL(first_url);
  // We don't sleep here - the first navigation won't have been committed yet
  // because we told the server to wait a minute. This means the browser has
  // started it's provisional load for the client redirect destination page but
  // hasn't completed. Our time is now!
  NavigateToURL(final_url);

  std::wstring tab_title;
  std::wstring final_url_title = UTF8ToWide("Title Of Awesomeness");
  // Wait till the final page has been loaded.
  for (int i = 0; i < 10; ++i) {
    base::PlatformThread::Sleep(TestTimeouts::action_timeout_ms());
    scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
    ASSERT_TRUE(tab_proxy.get());
    ASSERT_TRUE(tab_proxy->GetTabTitle(&tab_title));
    if (tab_title == final_url_title) {
      ASSERT_TRUE(tab_proxy->GetRedirectsFrom(first_url, &redirects));
      break;
    }
  }

  // Check to make sure the navigation did in fact take place and we are
  // at the expected page.
  EXPECT_EQ(final_url_title, tab_title);

  bool final_navigation_not_redirect = true;
  // Check to make sure our request for files/title2.html doesn't get flagged
  // as a client redirect from the first (/client-redirect?) page.
  for (std::vector<GURL>::iterator it = redirects.begin();
       it != redirects.end(); ++it) {
    if (final_url.spec() == it->spec()) {
      final_navigation_not_redirect = false;
      break;
    }
  }
  EXPECT_TRUE(final_navigation_not_redirect);
}

}  // namespace
