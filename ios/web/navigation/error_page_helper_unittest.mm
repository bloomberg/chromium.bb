// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/error_page_helper.h"

#include "base/strings/sys_string_conversions.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ErrorPageHelperTest = PlatformTest;

// Tests that the failed navigation URL is correctly extracted from the error.
TEST_F(ErrorPageHelperTest, FailedNavigationURL) {
  NSString* url_string = @"https://test-error-page.com";
  NSError* error = [NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorBadURL
             userInfo:@{NSURLErrorFailingURLStringErrorKey : url_string}];
  ErrorPageHelper* helper = [[ErrorPageHelper alloc] initWithError:error];
  NSURL* url = [NSURL URLWithString:url_string];
  EXPECT_NSEQ(url, helper.failedNavigationURL);
}

// Tests that the original URL is correctly extracted from the file error URL
// created by the helper.
TEST_F(ErrorPageHelperTest, ExtractOriginalURLFromErrorPageURL) {
  NSString* url_string = @"https://test-error-page.com";
  NSError* error = [NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorBadURL
             userInfo:@{NSURLErrorFailingURLStringErrorKey : url_string}];
  ErrorPageHelper* helper = [[ErrorPageHelper alloc] initWithError:error];
  GURL result_original_url = [ErrorPageHelper
      failedNavigationURLFromErrorPageFileURL:net::GURLWithNSURL(
                                                  helper.errorPageFileURL)];
  EXPECT_EQ(GURL(base::SysNSStringToUTF8(url_string)), result_original_url);
}

// Tests that the error page is correctly identified as error page.
TEST_F(ErrorPageHelperTest, IsErrorPageFileURL) {
  NSString* url_string = @"https://test-error-page.com";
  NSError* error = [NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorBadURL
             userInfo:@{NSURLErrorFailingURLStringErrorKey : url_string}];
  ErrorPageHelper* helper = [[ErrorPageHelper alloc] initWithError:error];
  EXPECT_TRUE([helper
      isErrorPageFileURLForFailedNavigationURL:helper.errorPageFileURL]);
}

// Tests that a normal URL isn't identified as error page.
TEST_F(ErrorPageHelperTest, IsErrorPageFileURLWrong) {
  NSString* url_string = @"file://test-error-page.com";
  NSError* error =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorBadURL
                      userInfo:@{
                        NSURLErrorFailingURLStringErrorKey : @"http://fake.com"
                      }];
  ErrorPageHelper* helper = [[ErrorPageHelper alloc] initWithError:error];
  EXPECT_FALSE([helper
      isErrorPageFileURLForFailedNavigationURL:[NSURL
                                                   URLWithString:url_string]]);
}

// Tests that the failed navigation URL is correctly extracted from the page
// URL.
TEST_F(ErrorPageHelperTest, FailedNavigationURLFromErrorPageFileURLCorrect) {
  std::string expected_url = "http://expected-url.com";
  std::string path = base::SysNSStringToUTF8([NSBundle.mainBundle
      pathForResource:@"error_page_loaded"
               ofType:@"html"]);

  std::string url_string = "file://" + path +
                           "?file=http://not-that-url.com&url=" + expected_url +
                           "&garbage=http://still-not-that-one.com";
  GURL result_url = [ErrorPageHelper
      failedNavigationURLFromErrorPageFileURL:GURL(url_string)];
  EXPECT_EQ(GURL(expected_url), result_url);
}

// Tests that the extract failed navigation URL is empty if the |url| query
// isn't present in the page URL.
TEST_F(ErrorPageHelperTest, FailedNavigationURLFromErrorPageFileURLNoQuery) {
  std::string expected_url = "http://expected-url.com";
  std::string path = base::SysNSStringToUTF8([NSBundle.mainBundle
      pathForResource:@"error_page_loaded"
               ofType:@"html"]);

  std::string url_string = "file://" + path + "?file=" + expected_url +
                           "&garbage=http://still-not-that-one.com";
  GURL result_url = [ErrorPageHelper
      failedNavigationURLFromErrorPageFileURL:GURL(url_string)];
  EXPECT_TRUE(result_url.is_empty());
}

// Tests that the extracted failed navigation URL is empty if the path of the
// current page isn't correct.
TEST_F(ErrorPageHelperTest, FailedNavigationURLFromErrorPageFileURLWrongPath) {
  std::string url_string =
      "file://not-the-correct-path.com?url=http://potential-url.com";
  GURL result_url = [ErrorPageHelper
      failedNavigationURLFromErrorPageFileURL:GURL(url_string)];
  EXPECT_TRUE(result_url.is_empty());
}

// Tests that the extracted failed navigation URL is empty if the scheme of the
// current page isn't file://.
TEST_F(ErrorPageHelperTest,
       FailedNavigationURLFromErrorPageFileURLWrongScheme) {
  std::string path = base::SysNSStringToUTF8([NSBundle.mainBundle
      pathForResource:@"error_page_loaded"
               ofType:@"html"]);

  std::string url_string = "http://" + path + "?url=http://potential-url.com";
  GURL result_url = [ErrorPageHelper
      failedNavigationURLFromErrorPageFileURL:GURL(url_string)];
  EXPECT_TRUE(result_url.is_empty());
}
