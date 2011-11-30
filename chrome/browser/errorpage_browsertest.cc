// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_navigation_observer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/net/url_request_failed_dns_job.h"
#include "content/browser/net/url_request_mock_http_job.h"

using content::BrowserThread;

class ErrorPageTest : public InProcessBrowserTest {
 public:
  enum HistoryNavigationDirection {
    HISTORY_NAVIGATE_BACK,
    HISTORY_NAVIGATE_FORWARD,
  };

  // Navigates the active tab to a mock url created for the file at |file_path|.
  void NavigateToFileURL(const FilePath::StringType& file_path) {
    ui_test_utils::NavigateToURL(
        browser(),
        URLRequestMockHTTPJob::GetMockUrl(FilePath(file_path)));
  }

  // Navigates to the given URL and waits for |num_navigations| to occur, and
  // the title to change to |expected_title|.
  void NavigateToURLAndWaitForTitle(const GURL& url,
                                    const std::string& expected_title,
                                    int num_navigations) {
    ui_test_utils::TitleWatcher title_watcher(
        browser()->GetSelectedTabContents(),
        ASCIIToUTF16(expected_title));

    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), url, num_navigations);

    EXPECT_EQ(title_watcher.WaitAndGetTitle(), ASCIIToUTF16(expected_title));
  }

  // Navigates back in the history and waits for |num_navigations| to occur, and
  // the title to change to |expected_title|.
  void GoBackAndWaitForTitle(const std::string& expected_title,
                             int num_navigations) {
    NavigateHistoryAndWaitForTitle(expected_title,
                                   num_navigations,
                                   HISTORY_NAVIGATE_BACK);
  }

  // Navigates forward in the history and waits for |num_navigations| to occur,
  // and the title to change to |expected_title|.
  void GoForwardAndWaitForTitle(const std::string& expected_title,
                                int num_navigations) {
    NavigateHistoryAndWaitForTitle(expected_title,
                                   num_navigations,
                                   HISTORY_NAVIGATE_FORWARD);
  }

 protected:
  void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
  }

 private:
  // Navigates the browser the indicated direction in the history and waits for
  // |num_navigations| to occur and the title to change to |expected_title|.
  void NavigateHistoryAndWaitForTitle(const std::string& expected_title,
                                      int num_navigations,
                                      HistoryNavigationDirection direction) {
    ui_test_utils::TitleWatcher title_watcher(
        browser()->GetSelectedTabContents(),
        ASCIIToUTF16(expected_title));

    TestNavigationObserver test_navigation_observer(
        content::Source<NavigationController>(
            &browser()->GetSelectedTabContentsWrapper()->controller()),
        NULL,
        num_navigations);
    if (direction == HISTORY_NAVIGATE_BACK) {
      browser()->GoBack(CURRENT_TAB);
    } else if (direction == HISTORY_NAVIGATE_FORWARD) {
      browser()->GoForward(CURRENT_TAB);
    } else {
      FAIL();
    }
    test_navigation_observer.WaitForObservation();

    EXPECT_EQ(title_watcher.WaitAndGetTitle(), ASCIIToUTF16(expected_title));
  }
};

// Test that a DNS error occuring in the main frame redirects to an error page.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_Basic) {
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);
  // The first navigation should fail, and the second one should be the error
  // page.
  NavigateToURLAndWaitForTitle(test_url, "Mock Link Doctor", 2);
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack1) {
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));
  NavigateToURLAndWaitForTitle(test_url, "Mock Link Doctor", 2);
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack2) {
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));

  NavigateToURLAndWaitForTitle(test_url, "Mock Link Doctor", 2);
  NavigateToFileURL(FILE_PATH_LITERAL("title3.html"));

  GoBackAndWaitForTitle("Mock Link Doctor", 2);
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack2AndForward) {
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));

  NavigateToURLAndWaitForTitle(test_url, "Mock Link Doctor", 2);
  NavigateToFileURL(FILE_PATH_LITERAL("title3.html"));

  GoBackAndWaitForTitle("Mock Link Doctor", 2);
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);

  GoForwardAndWaitForTitle("Mock Link Doctor", 2);
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack2Forward2) {
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);
  NavigateToFileURL(FILE_PATH_LITERAL("title3.html"));

  NavigateToURLAndWaitForTitle(test_url, "Mock Link Doctor", 2);
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));

  GoBackAndWaitForTitle("Mock Link Doctor", 2);
  GoBackAndWaitForTitle("Title Of More Awesomeness", 1);

  GoForwardAndWaitForTitle("Mock Link Doctor", 2);
  GoForwardAndWaitForTitle("Title Of Awesomeness", 1);
}

// Test that a DNS error occuring in an iframe.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, IFrameDNSError_Basic) {
  NavigateToURLAndWaitForTitle(
      URLRequestMockHTTPJob::GetMockUrl(
          FilePath(FILE_PATH_LITERAL("iframe_dns_error.html"))),
      "Blah",
      1);
}

// Test that a DNS error occuring in an iframe does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, IFrameDNSError_GoBack) {
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));
  NavigateToFileURL(FILE_PATH_LITERAL("iframe_dns_error.html"));
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
}

// Test that a DNS error occuring in an iframe does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, IFrameDNSError_GoBackAndForward) {
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));
  NavigateToFileURL(FILE_PATH_LITERAL("iframe_dns_error.html"));
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
  GoForwardAndWaitForTitle("Blah", 1);
}

// Checks that the Link Doctor is not loaded when we receive an actual 404 page.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, Page404) {
  NavigateToURLAndWaitForTitle(
      URLRequestMockHTTPJob::GetMockUrl(
          FilePath(FILE_PATH_LITERAL("page404.html"))),
      "SUCCESS",
      1);
}
