// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/net/url_request_failed_dns_job.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/window_open_disposition.h"

namespace {

class ErrorPageBrowserTest : public InProcessBrowserTest {
};

using ui_test_utils::NavigateToURL;

// We want to wait for two navigations: first will be the failing one,
// and the second will be the error page.
using ui_test_utils::NavigateToURLBlockUntilNavigationsComplete;

IN_PROC_BROWSER_TEST_F(ErrorPageBrowserTest, DNSError_Basic) {
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);
  string16 title;

  NavigateToURLBlockUntilNavigationsComplete(browser(), test_url, 2);
  EXPECT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
  EXPECT_EQ(ASCIIToUTF16("Mock Link Doctor"), title);
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageBrowserTest, DNSError_GoBack1) {
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);
  string16 title;

  NavigateToURL(browser(), URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));
  NavigateToURLBlockUntilNavigationsComplete(browser(), test_url, 2);
  EXPECT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
  EXPECT_EQ(ASCIIToUTF16("Mock Link Doctor"), title);

  browser()->GoBack(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));

  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
  EXPECT_EQ(ASCIIToUTF16("Title Of Awesomeness"), title);
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageBrowserTest, DNSError_GoBack2) {
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);
  string16 title;

  NavigateToURL(browser(), URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));
  NavigateToURLBlockUntilNavigationsComplete(browser(), test_url, 2);
  EXPECT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
  EXPECT_EQ(ASCIIToUTF16("Mock Link Doctor"), title);
  NavigateToURL(browser(), URLRequestMockHTTPJob::GetMockUrl(L"title3.html"));

  browser()->GoBack(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationsInCurrentTab(browser(), 2));
  EXPECT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
  EXPECT_EQ(ASCIIToUTF16("Mock Link Doctor"), title);

  browser()->GoBack(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));

  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
  EXPECT_EQ(ASCIIToUTF16("Title Of Awesomeness"), title);
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageBrowserTest, DNSError_GoBackAndForward1) {
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);
  string16 title;

  NavigateToURL(browser(), URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));
  NavigateToURLBlockUntilNavigationsComplete(browser(), test_url, 2);
  EXPECT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
  EXPECT_EQ(ASCIIToUTF16("Mock Link Doctor"), title);
  NavigateToURL(browser(), URLRequestMockHTTPJob::GetMockUrl(L"title3.html"));

  browser()->GoBack(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationsInCurrentTab(browser(), 2));
  EXPECT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
  EXPECT_EQ(ASCIIToUTF16("Mock Link Doctor"), title);

  browser()->GoBack(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));

  browser()->GoForward(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationsInCurrentTab(browser(), 2));
}

// Test that a DNS error occuring in the main frame does not result in an
// additional session history entry.
IN_PROC_BROWSER_TEST_F(ErrorPageBrowserTest, DNSError_GoBackAndForward2) {
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);
  string16 title;

  NavigateToURL(browser(), URLRequestMockHTTPJob::GetMockUrl(L"title3.html"));
  NavigateToURLBlockUntilNavigationsComplete(browser(), test_url, 2);
  EXPECT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
  EXPECT_EQ(ASCIIToUTF16("Mock Link Doctor"), title);
  NavigateToURL(browser(), URLRequestMockHTTPJob::GetMockUrl(L"title2.html"));

  browser()->GoBack(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationsInCurrentTab(browser(), 2));
  EXPECT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
  EXPECT_EQ(ASCIIToUTF16("Mock Link Doctor"), title);

  browser()->GoBack(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));

  browser()->GoForward(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationsInCurrentTab(browser(), 2));
  EXPECT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
  EXPECT_EQ(ASCIIToUTF16("Mock Link Doctor"), title);

  browser()->GoForward(CURRENT_TAB);
  ASSERT_TRUE(ui_test_utils::WaitForNavigationInCurrentTab(browser()));

  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
  EXPECT_EQ(ASCIIToUTF16("Title Of Awesomeness"), title);
}

}  // namespace
