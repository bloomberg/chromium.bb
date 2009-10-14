// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#if defined(OS_WIN)
#include "base/win_util.h"
#endif  // defined(OS_WIN)
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/browser/net/url_request_failed_dns_job.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "net/url_request/url_request_unittest.h"

class ErrorPageTest : public UITest {
 protected:
  bool WaitForTitleMatching(const std::wstring& title) {
    for (int i = 0; i < 100; ++i) {
      if (GetActiveTabTitle() == title)
        return true;
      PlatformThread::Sleep(sleep_timeout_ms() / 10);
    }
    EXPECT_EQ(title, GetActiveTabTitle());
    return false;
  }
};

TEST_F(ErrorPageTest, DNSError_Basic) {
#if defined(OS_WIN)
  // Flaky on XP, http://crbug.com/19361.
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)
    return;
#endif  // defined(OS_WIN)
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);

  // The first navigation should fail, and the second one should be the error
  // page.
  NavigateToURLBlockUntilNavigationsComplete(test_url, 2);

  EXPECT_TRUE(WaitForTitleMatching(L"Mock Link Doctor"));
}

TEST_F(ErrorPageTest, DNSError_GoBack1) {
#if defined(OS_WIN)
  // Flaky on XP, http://crbug.com/19361.
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)
    return;
#endif  // defined(OS_WIN)
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

TEST_F(ErrorPageTest, DNSError_GoBack2) {
#if defined(OS_WIN)
  // Flaky on XP, http://crbug.com/19361.
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)
    return;
#endif  // defined(OS_WIN)
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

TEST_F(ErrorPageTest, DNSError_GoBack2AndForward) {
#if defined(OS_WIN)
  // Flaky on XP, http://crbug.com/19361.
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)
    return;
#endif  // defined(OS_WIN)
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

TEST_F(ErrorPageTest, DNSError_GoBack2Forward2) {
#if defined(OS_WIN)
  // Flaky on XP, http://crbug.com/19361.
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)
    return;
#endif  // defined(OS_WIN)
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
#if defined(OS_WIN)
  // Flaky on XP, http://crbug.com/19361.
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)
    return;
#endif  // defined(OS_WIN)
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("iframe_dns_error.html"))));
  EXPECT_TRUE(WaitForTitleMatching(L"Blah"));
}

TEST_F(ErrorPageTest, IFrameDNSError_GoBack) {
#if defined(OS_WIN)
  // Flaky on XP, http://crbug.com/19361.
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)
    return;
#endif  // defined(OS_WIN)
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
#if defined(OS_WIN)
  // Flaky on XP, http://crbug.com/19361.
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)
    return;
#endif  // defined(OS_WIN)
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

TEST_F(ErrorPageTest, IFrame404) {
#if defined(OS_WIN)
  // Flaky on XP, http://crbug.com/19361.
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)
    return;
#endif  // defined(OS_WIN)
  // iframes that have 404 pages should not trigger an alternate error page.
  // In this test, the iframe sets the title of the parent page to "SUCCESS"
  // when the iframe loads.  If the iframe fails to load (because an alternate
  // error page loads instead), then the title will remain as "FAIL".
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"chrome/test/data", NULL);
  ASSERT_TRUE(NULL != server.get());
  GURL test_url = server->TestServerPage("files/iframe404.html");
  NavigateToURL(test_url);

  EXPECT_TRUE(WaitForTitleMatching(L"SUCCESS"));
}

TEST_F(ErrorPageTest, Page404) {
#if defined(OS_WIN)
  // Flaky on XP, http://crbug.com/19361.
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)
    return;
#endif  // defined(OS_WIN)
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("title2.html"))));
  // The first navigation should fail, and the second one should be the error
  // page.
  NavigateToURLBlockUntilNavigationsComplete(
      URLRequestMockHTTPJob::GetMockUrl(
          FilePath(FILE_PATH_LITERAL("page404.html"))), 2);

  EXPECT_TRUE(WaitForTitleMatching(L"Mock Link Doctor"));
}

TEST_F(ErrorPageTest, Page404_GoBack) {
#if defined(OS_WIN)
  // Flaky on XP, http://crbug.com/19361.
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA)
    return;
#endif  // defined(OS_WIN)
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                    FilePath(FILE_PATH_LITERAL("title2.html"))));
  // The first navigation should fail, and the second one should be the error
  // page.
  NavigateToURLBlockUntilNavigationsComplete(
      URLRequestMockHTTPJob::GetMockUrl(
          FilePath(FILE_PATH_LITERAL("page404.html"))), 2);
  EXPECT_TRUE(WaitForTitleMatching(L"Mock Link Doctor"));

  EXPECT_TRUE(GetActiveTab()->GoBack());

  EXPECT_TRUE(WaitForTitleMatching(L"Title Of Awesomeness"));
}
