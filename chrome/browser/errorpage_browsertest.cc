// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/test/net/url_request_failed_job.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_job_factory.h"

using content::BrowserThread;
using content::NavigationController;
using content::URLRequestFailedJob;

namespace {

class ErrorPageTest : public InProcessBrowserTest {
 public:
  enum HistoryNavigationDirection {
    HISTORY_NAVIGATE_BACK,
    HISTORY_NAVIGATE_FORWARD,
  };

  // Navigates the active tab to a mock url created for the file at |file_path|.
  void NavigateToFileURL(const base::FilePath::StringType& file_path) {
    ui_test_utils::NavigateToURL(
        browser(),
        content::URLRequestMockHTTPJob::GetMockUrl(base::FilePath(file_path)));
  }

  // Navigates to the given URL and waits for |num_navigations| to occur, and
  // the title to change to |expected_title|.
  void NavigateToURLAndWaitForTitle(const GURL& url,
                                    const std::string& expected_title,
                                    int num_navigations) {
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(),
        ASCIIToUTF16(expected_title));

    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), url, num_navigations);

    EXPECT_EQ(ASCIIToUTF16(expected_title), title_watcher.WaitAndGetTitle());
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
  virtual void SetUpOnMainThread() OVERRIDE {
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
        browser()->tab_strip_model()->GetActiveWebContents(),
        ASCIIToUTF16(expected_title));

    content::TestNavigationObserver test_navigation_observer(
        content::Source<NavigationController>(
              &browser()->tab_strip_model()->GetActiveWebContents()->
                  GetController()),
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

// See crbug.com/109669
#if defined(USE_AURA)
#define MAYBE_DNSError_GoBack1 DISABLED_DNSError_GoBack1
#else
#define MAYBE_DNSError_GoBack1 DNSError_GoBack1
#endif

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageTest, MAYBE_DNSError_GoBack1) {
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
      content::URLRequestMockHTTPJob::GetMockUrl(
          base::FilePath(FILE_PATH_LITERAL("iframe_dns_error.html"))),
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
      content::URLRequestMockHTTPJob::GetMockUrl(
          base::FilePath(FILE_PATH_LITERAL("page404.html"))),
      "SUCCESS",
      1);
}

// Protocol handler that fails all requests with net::ERR_ADDRESS_UNREACHABLE.
class AddressUnreachableProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  AddressUnreachableProtocolHandler() {}
  virtual ~AddressUnreachableProtocolHandler() {}

  // net::URLRequestJobFactory::ProtocolHandler:
  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    return new URLRequestFailedJob(request,
                                   network_delegate,
                                   net::ERR_ADDRESS_UNREACHABLE);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AddressUnreachableProtocolHandler);
};

// A test fixture that returns ERR_ADDRESS_UNREACHABLE for all Link Doctor
// requests.  ERR_NAME_NOT_RESOLVED is more typical, but need to use a different
// error for the Link Doctor and the original page to validate the right page
// is being displayed.
class ErrorPageLinkDoctorFailTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ErrorPageLinkDoctorFailTest::AddFilters));
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ErrorPageLinkDoctorFailTest::RemoveFilters));
  }

 private:
  // Adds a filter that causes all requests for the Link Doctor's scheme and
  // host to fail with ERR_ADDRESS_UNREACHABLE.  Since the Link Doctor adds
  // query strings, it's not enough to just fail exact matches.
  //
  // Also adds the content::URLRequestFailedJob filter.
  static void AddFilters() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    content::URLRequestFailedJob::AddUrlHandler();

    net::URLRequestFilter::GetInstance()->AddHostnameProtocolHandler(
        google_util::LinkDoctorBaseURL().scheme(),
        google_util::LinkDoctorBaseURL().host(),
        scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>(
            new AddressUnreachableProtocolHandler()));
  }

  static void RemoveFilters() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }
};

// Make sure that when the Link Doctor fails to load, the network error page is
// successfully loaded.
IN_PROC_BROWSER_TEST_F(ErrorPageLinkDoctorFailTest, LinkDoctorFail) {
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED),
      2);

  // Verify that the expected error page is being displayed.  Do this by making
  // sure the original error code (ERR_NAME_NOT_RESOLVED) is displayed.
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "var textContent = document.body.textContent;"
      "var hasError = textContent.indexOf('ERR_NAME_NOT_RESOLVED') >= 0;"
      "domAutomationController.send(hasError);",
      &result));
  EXPECT_TRUE(result);
}

}  // namespace
