// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/chrome_url_util.h"

#include "ios/chrome/browser/chrome_url_constants.h"
#import "net/base/mac/url_conversions.h"
#import "testing/gtest_mac.h"
#include "url/gurl.h"

namespace {

TEST(ChromeURLUtilTest, TestIsExternalFileReference) {
  GURL external_url("chrome://external-file/foo/bar");
  GURL not_external_url("chrome://foo/bar");
  GURL still_not_external_url("http://external-file/foo/bar");
  EXPECT_TRUE(UrlIsExternalFileReference(external_url));
  EXPECT_FALSE(UrlIsExternalFileReference(not_external_url));
  EXPECT_FALSE(UrlIsExternalFileReference(still_not_external_url));
}

TEST(ChromeURLUtilTest, TestRewriteURLChromium) {
  [[ChromeAppConstants sharedInstance] setCallbackSchemeForTesting:@"chromium"];
  NSURL* expected = [NSURL URLWithString:@"chromium://"];
  NSURL* rewritten = UrlToLaunchChrome();
  EXPECT_NSEQ([expected absoluteString], [rewritten absoluteString]);
}

TEST(ChromeURLUtilTest, TestRewriteURLGoogleChrome) {
  [[ChromeAppConstants sharedInstance]
      setCallbackSchemeForTesting:@"googlechrome"];
  NSURL* expected = [NSURL URLWithString:@"googlechrome://"];
  NSURL* rewritten = UrlToLaunchChrome();
  EXPECT_NSEQ([expected absoluteString], [rewritten absoluteString]);
}

TEST(ChromeURLUtilTest, TestAppIconURL) {
  [[ChromeAppConstants sharedInstance] setAppIconURLProviderForTesting:nil];
  NSURL* url = UrlOfChromeAppIcon(29, 29);
  EXPECT_TRUE(url);
  GURL gurl(net::GURLWithNSURL(url));
  EXPECT_TRUE(gurl.is_valid());
}

const char* kSchemeTestData[] = {
    "http://foo.com",
    "https://foo.com",
    "data:text/html;charset=utf-8,Hello",
    "about:blank",
    "chrome://settings",
};

TEST(ChromeURLUtilTest, NSURLHasChromeScheme) {
  for (unsigned int i = 0; i < arraysize(kSchemeTestData); ++i) {
    const char* url = kSchemeTestData[i];
    bool nsurl_result = UrlHasChromeScheme(
        [NSURL URLWithString:[NSString stringWithUTF8String:url]]);
    bool gurl_result = GURL(url).SchemeIs(kChromeUIScheme);
    EXPECT_EQ(gurl_result, nsurl_result) << "Scheme check failed for " << url;
  }
}

}  // namespace
