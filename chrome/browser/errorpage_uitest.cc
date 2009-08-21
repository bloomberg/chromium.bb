// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
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
    return false;
  }
  bool WaitForTitleContaining(const std::string& title_substring) {
    for (int i = 0; i < 100; ++i) {
      std::wstring title = GetActiveTabTitle();
      if (title.find(UTF8ToWide(title_substring)) != std::wstring::npos)
        return true;
      PlatformThread::Sleep(sleep_timeout_ms() / 10);
    }
    return false;
  }
};

TEST_F(ErrorPageTest, DNSError_Basic) {
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);

  NavigateToURLBlockUntilNavigationsComplete(test_url, 2);

  EXPECT_TRUE(WaitForTitleContaining(test_url.host()));
}

// Flaky, see http://crbug.com/19361 and http://crbug.com/19395.
TEST_F(ErrorPageTest, DISABLED_DNSError_GoBack1) {
  // Test that a DNS error occuring in the main frame does not result in an
  // additional session history entry.
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));
  NavigateToURL(test_url);
  EXPECT_TRUE(WaitForTitleContaining(test_url.host()));

  GetActiveTab()->GoBack();

  EXPECT_TRUE(WaitForTitleMatching(L"Title Of Awesomeness"));
}

// Flaky, see http://crbug.com/19361 and http://crbug.com/19395.
TEST_F(ErrorPageTest, DISABLED_DNSError_GoBack2) {
  // Test that a DNS error occuring in the main frame does not result in an
  // additional session history entry.
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));
  NavigateToURL(test_url);
  EXPECT_TRUE(WaitForTitleContaining(test_url.host()));
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"title3.html"));

  GetActiveTab()->GoBack();
  EXPECT_TRUE(WaitForTitleContaining(test_url.host()));
  GetActiveTab()->GoBack();

  EXPECT_TRUE(WaitForTitleMatching(L"Title Of Awesomeness"));
}

// Flaky, see http://crbug.com/19361 and http://crbug.com/19395.
TEST_F(ErrorPageTest, DISABLED_DNSError_GoBack2AndForward) {
  // Test that a DNS error occuring in the main frame does not result in an
  // additional session history entry.

  GURL test_url(URLRequestFailedDnsJob::kTestUrl);

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));
  NavigateToURL(test_url);
  EXPECT_TRUE(WaitForTitleContaining(test_url.host()));
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"title3.html"));

  GetActiveTab()->GoBack();
  EXPECT_TRUE(WaitForTitleContaining(test_url.host()));
  GetActiveTab()->GoBack();
  GetActiveTab()->GoForward();

  EXPECT_TRUE(WaitForTitleContaining(test_url.host()));
}

// Flaky, see http://crbug.com/19361 and http://crbug.com/19395.
TEST_F(ErrorPageTest, DISABLED_DNSError_GoBack2Forward2) {
  // Test that a DNS error occuring in the main frame does not result in an
  // additional session history entry.

  GURL test_url(URLRequestFailedDnsJob::kTestUrl);

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"title3.html"));
  NavigateToURL(test_url);
  EXPECT_TRUE(WaitForTitleContaining(test_url.host()));
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));

  GetActiveTab()->GoBack();
  EXPECT_TRUE(WaitForTitleContaining(test_url.host()));
  GetActiveTab()->GoBack();
  GetActiveTab()->GoForward();
  EXPECT_TRUE(WaitForTitleContaining(test_url.host()));
  GetActiveTab()->GoForward();

  EXPECT_TRUE(WaitForTitleMatching(L"Title Of Awesomeness"));
}

TEST_F(ErrorPageTest, IFrameDNSError_Basic) {
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"iframe_dns_error.html"));
  EXPECT_TRUE(WaitForTitleMatching(L"Blah"));
}

TEST_F(ErrorPageTest, IFrameDNSError_GoBack) {
  // Test that a DNS error occuring in an iframe does not result in an
  // additional session history entry.

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"iframe_dns_error.html"));

  GetActiveTab()->GoBack();

  EXPECT_TRUE(WaitForTitleMatching(L"Title Of Awesomeness"));
}

TEST_F(ErrorPageTest, IFrameDNSError_GoBackAndForward) {
  // Test that a DNS error occuring in an iframe does not result in an
  // additional session history entry.

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"iframe_dns_error.html"));

  GetActiveTab()->GoBack();
  GetActiveTab()->GoForward();

  EXPECT_TRUE(WaitForTitleMatching(L"Blah"));
}

TEST_F(ErrorPageTest, IFrame404) {
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

#if defined(OS_LINUX)
// TODO(phajdan.jr): This test is flaky on Linux, http://crbug.com/19361
#define Page404 DISABLED_Page404
#define Page404_GoBack DISABLED_Page404_GoBack
#endif

TEST_F(ErrorPageTest, Page404) {
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"page404.html"));

  EXPECT_TRUE(WaitForTitleContaining("page404.html"));
}

TEST_F(ErrorPageTest, Page404_GoBack) {
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));
  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(L"page404.html"));
  EXPECT_TRUE(WaitForTitleContaining("page404.html"));

  GetActiveTab()->GoBack();

  EXPECT_TRUE(WaitForTitleMatching(L"Title Of Awesomeness"));
}
