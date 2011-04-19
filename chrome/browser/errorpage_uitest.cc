// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/browser/net/url_request_failed_dns_job.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "net/test/test_server.h"

class ErrorPageTest : public UITest {
 protected:
  bool WaitForTitleMatching(const std::wstring& title) {
    for (int i = 0; i < 10; ++i) {
      if (GetActiveTabTitle() == title)
        return true;
      base::PlatformThread::Sleep(TestTimeouts::action_timeout_ms());
    }
    EXPECT_EQ(title, GetActiveTabTitle());
    return false;
  }
};

TEST_F(ErrorPageTest, DNSError_Basic) {
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);

  // The first navigation should fail, and the second one should be the error
  // page.
  NavigateToURLBlockUntilNavigationsComplete(test_url, 2);

  EXPECT_TRUE(WaitForTitleMatching(L"Mock Link Doctor"));
}

TEST_F(ErrorPageTest, DNSError_GoBack1) {
  // Test that a DNS error occuring in the main frame does not result in an
  // additional session history entry.
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("title2.html"))));
  // The first navigation should fail, and the second one should be the error
  // page.
  NavigateToURLBlockUntilNavigationsComplete(test_url, 2);
  EXPECT_TRUE(WaitForTitleMatching(L"Mock Link Doctor"));

  EXPECT_TRUE(GetActiveTab()->GoBack());

  EXPECT_TRUE(WaitForTitleMatching(L"Title Of Awesomeness"));
}

// Flaky on Linux, see http://crbug.com/19361
#if defined(OS_LINUX)
#define MAYBE_DNSError_GoBack2 FLAKY_DNSError_GoBack2
#else
#define MAYBE_DNSError_GoBack2 DNSError_GoBack2
#endif
TEST_F(ErrorPageTest, MAYBE_DNSError_GoBack2) {
  // Test that a DNS error occuring in the main frame does not result in an
  // additional session history entry.
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("title2.html"))));
  // The first navigation should fail, and the second one should be the error
  // page.
  NavigateToURLBlockUntilNavigationsComplete(test_url, 2);
  EXPECT_TRUE(WaitForTitleMatching(L"Mock Link Doctor"));
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("title3.html"))));

  // The first navigation should fail, and the second one should be the error
  // page.
  EXPECT_TRUE(GetActiveTab()->GoBackBlockUntilNavigationsComplete(2));
  EXPECT_TRUE(WaitForTitleMatching(L"Mock Link Doctor"));
  EXPECT_TRUE(GetActiveTab()->GoBack());

  EXPECT_TRUE(WaitForTitleMatching(L"Title Of Awesomeness"));
}

// Flaky on Linux, see http://crbug.com/19361
#if defined(OS_LINUX)
#define MAYBE_DNSError_GoBack2AndForward FLAKY_DNSError_GoBack2AndForward
#else
#define MAYBE_DNSError_GoBack2AndForward DNSError_GoBack2AndForward
#endif
TEST_F(ErrorPageTest, MAYBE_DNSError_GoBack2AndForward) {
  // Test that a DNS error occuring in the main frame does not result in an
  // additional session history entry.

  GURL test_url(URLRequestFailedDnsJob::kTestUrl);

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("title2.html"))));
  // The first navigation should fail, and the second one should be the error
  // page.
  NavigateToURLBlockUntilNavigationsComplete(test_url, 2);
  EXPECT_TRUE(WaitForTitleMatching(L"Mock Link Doctor"));
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("title3.html"))));

  // The first navigation should fail, and the second one should be the error
  // page.
  EXPECT_TRUE(GetActiveTab()->GoBackBlockUntilNavigationsComplete(2));
  EXPECT_TRUE(WaitForTitleMatching(L"Mock Link Doctor"));
  EXPECT_TRUE(GetActiveTab()->GoBack());
  // The first navigation should fail, and the second one should be the error
  // page.
  EXPECT_TRUE(GetActiveTab()->GoForwardBlockUntilNavigationsComplete(2));

  EXPECT_TRUE(WaitForTitleMatching(L"Mock Link Doctor"));
}

// Flaky on Linux, see http://crbug.com/19361
#if defined(OS_LINUX)
#define MAYBE_DNSError_GoBack2Forward2 FLAKY_DNSError_GoBack2Forward2
#else
#define MAYBE_DNSError_GoBack2Forward2 DNSError_GoBack2Forward2
#endif
TEST_F(ErrorPageTest, MAYBE_DNSError_GoBack2Forward2) {
  // Test that a DNS error occuring in the main frame does not result in an
  // additional session history entry.

  GURL test_url(URLRequestFailedDnsJob::kTestUrl);

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("title3.html"))));
  // The first navigation should fail, and the second one should be the error
  // page.
  NavigateToURLBlockUntilNavigationsComplete(test_url, 2);
  EXPECT_TRUE(WaitForTitleMatching(L"Mock Link Doctor"));
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("title2.html"))));

  // The first navigation should fail, and the second one should be the error
  // page.
  EXPECT_TRUE(GetActiveTab()->GoBackBlockUntilNavigationsComplete(2));
  EXPECT_TRUE(WaitForTitleMatching(L"Mock Link Doctor"));
  EXPECT_TRUE(GetActiveTab()->GoBack());
  // The first navigation should fail, and the second one should be the error
  // page.
  EXPECT_TRUE(GetActiveTab()->GoForwardBlockUntilNavigationsComplete(2));
  EXPECT_TRUE(WaitForTitleMatching(L"Mock Link Doctor"));
  EXPECT_TRUE(GetActiveTab()->GoForward());

  EXPECT_TRUE(WaitForTitleMatching(L"Title Of Awesomeness"));
}

TEST_F(ErrorPageTest, IFrameDNSError_Basic) {
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("iframe_dns_error.html"))));
  EXPECT_TRUE(WaitForTitleMatching(L"Blah"));
}

TEST_F(ErrorPageTest, IFrameDNSError_GoBack) {
  // Test that a DNS error occuring in an iframe does not result in an
  // additional session history entry.

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("title2.html"))));
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("iframe_dns_error.html"))));

  EXPECT_TRUE(GetActiveTab()->GoBack());

  EXPECT_TRUE(WaitForTitleMatching(L"Title Of Awesomeness"));
}

TEST_F(ErrorPageTest, IFrameDNSError_GoBackAndForward) {
  // Test that a DNS error occuring in an iframe does not result in an
  // additional session history entry.

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("title2.html"))));
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("iframe_dns_error.html"))));

  EXPECT_TRUE(GetActiveTab()->GoBack());
  EXPECT_TRUE(GetActiveTab()->GoForward());

  EXPECT_TRUE(WaitForTitleMatching(L"Blah"));
}

// Checks that the Link Doctor is not loaded when we receive an actual 404 page.
TEST_F(ErrorPageTest, Page404) {
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("page404.html"))));

  EXPECT_TRUE(WaitForTitleMatching(L"SUCCESS"));
}
