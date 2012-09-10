// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/test/net/url_request_failed_job.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "net/base/net_errors.h"

using content::BrowserThread;
using content::NavigationController;

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
    content::TitleWatcher title_watcher(
        chrome::GetActiveWebContents(browser()),
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

  // Returns a GURL that results in a DNS error.
  GURL GetDnsErrorURL() const {
    return URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);
  }

 private:
  // Navigates the browser the indicated direction in the history and waits for
  // |num_navigations| to occur and the title to change to |expected_title|.
  void NavigateHistoryAndWaitForTitle(const std::string& expected_title,
                                      int num_navigations,
                                      HistoryNavigationDirection direction) {
    content::TitleWatcher title_watcher(
        chrome::GetActiveWebContents(browser()),
        ASCIIToUTF16(expected_title));

    content::TestNavigationObserver test_navigation_observer(
        content::Source<NavigationController>(
              &chrome::GetActiveWebContents(browser())->GetController()),
        NULL,
        num_navigations);
    if (direction == HISTORY_NAVIGATE_BACK) {
      chrome::GoBack(browser(), CURRENT_TAB);
    } else if (direction == HISTORY_NAVIGATE_FORWARD) {
      chrome::GoForward(browser(), CURRENT_TAB);
    } else {
      FAIL();
    }
    test_navigation_observer.WaitForObservation(
        base::Bind(&content::RunMessageLoop),
        base::Bind(&MessageLoop::Quit,
                   base::Unretained(MessageLoopForUI::current())));

    EXPECT_EQ(title_watcher.WaitAndGetTitle(), ASCIIToUTF16(expected_title));
  }
};

// See crbug.com/109669
#if defined(USE_AURA)
#define MAYBE_DNSError_Basic DISABLED_DNSError_Basic
#else
#define MAYBE_DNSError_Basic DNSError_Basic
#endif
// Test that a DNS error occuring in the main frame redirects to an error page.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, MAYBE_DNSError_Basic) {
  // The first navigation should fail, and the second one should be the error
  // page.
  NavigateToURLAndWaitForTitle(GetDnsErrorURL(), "Mock Link Doctor", 2);
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack1) {
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));
  NavigateToURLAndWaitForTitle(GetDnsErrorURL(), "Mock Link Doctor", 2);
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
}

// See crbug.com/109669
#if defined(USE_AURA)
#define MAYBE_DNSError_GoBack2 DISABLED_DNSError_GoBack2
#else
#define MAYBE_DNSError_GoBack2 DNSError_GoBack2
#endif
// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack2) {
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));

  NavigateToURLAndWaitForTitle(GetDnsErrorURL(), "Mock Link Doctor", 2);
  NavigateToFileURL(FILE_PATH_LITERAL("title3.html"));

  GoBackAndWaitForTitle("Mock Link Doctor", 2);
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
}

// See crbug.com/109669
#if defined(USE_AURA)
#define MAYBE_DNSError_GoBack2AndForward DISABLED_DNSError_GoBack2AndForward
#else
#define MAYBE_DNSError_GoBack2AndForward DNSError_GoBack2AndForward
#endif
// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack2AndForward) {
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));

  NavigateToURLAndWaitForTitle(GetDnsErrorURL(), "Mock Link Doctor", 2);
  NavigateToFileURL(FILE_PATH_LITERAL("title3.html"));

  GoBackAndWaitForTitle("Mock Link Doctor", 2);
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);

  GoForwardAndWaitForTitle("Mock Link Doctor", 2);
}

// See crbug.com/109669
#if defined(USE_AURA)
#define MAYBE_DNSError_GoBack2Forward2 DISABLED_DNSError_GoBack2Forward2
#else
#define MAYBE_DNSError_GoBack2Forward2 DNSError_GoBack2Forward2
#endif
// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, DNSError_GoBack2Forward2) {
  NavigateToFileURL(FILE_PATH_LITERAL("title3.html"));

  NavigateToURLAndWaitForTitle(GetDnsErrorURL(), "Mock Link Doctor", 2);
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

// This test fails regularly on win_rel trybots. See crbug.com/121540
#if defined(OS_WIN)
#define MAYBE_IFrameDNSError_GoBack DISABLED_IFrameDNSError_GoBack
#else
#define MAYBE_IFrameDNSError_GoBack IFrameDNSError_GoBack
#endif
// Test that a DNS error occuring in an iframe does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, MAYBE_IFrameDNSError_GoBack) {
  NavigateToFileURL(FILE_PATH_LITERAL("title2.html"));
  NavigateToFileURL(FILE_PATH_LITERAL("iframe_dns_error.html"));
  GoBackAndWaitForTitle("Title Of Awesomeness", 1);
}

// This test fails regularly on win_rel trybots. See crbug.com/121540
#if defined(OS_WIN)
#define MAYBE_IFrameDNSError_GoBackAndForward DISABLED_IFrameDNSError_GoBackAndForward
#else
#define MAYBE_IFrameDNSError_GoBackAndForward IFrameDNSError_GoBackAndForward
#endif
// Test that a DNS error occuring in an iframe does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, MAYBE_IFrameDNSError_GoBackAndForward) {
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
