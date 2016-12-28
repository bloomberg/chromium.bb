// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/chrome_app_startup_parameters.h"

#import <Foundation/Foundation.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/stringprintf.h"
#include "ios/chrome/browser/app_startup_parameters.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/xcallback_parameters.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

namespace {
void CheckLaunchSourceForURL(first_run::ExternalLaunch expectedSource,
                             NSString* urlString) {
  NSURL* url = [NSURL URLWithString:urlString];
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters
          newChromeAppStartupParametersWithURL:url
                         fromSourceApplication:@"com.apple.mobilesafari"]);
  EXPECT_EQ(expectedSource, [params launchSource]);
}

typedef PlatformTest AppStartupParametersTest;
TEST_F(PlatformTest, ParseURLWithEmptyURL) {
  NSURL* url = [NSURL URLWithString:@""];
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil]);

  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithOneProtocol) {
  NSURL* url = [NSURL URLWithString:@"protocol://www.google.com"];
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil]);
  // Here "protocol" opens the app and no protocol is given for the parsed URL,
  // which defaults to be "http".
  EXPECT_EQ("http://www.google.com/", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithEmptyParsedURL) {
  // Test chromium://
  NSURL* url = [NSURL URLWithString:@"chromium://"];
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil]);

  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithParsedURLDefaultToHttp) {
  NSURL* url = [NSURL URLWithString:@"chromium://www.google.com"];
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil]);

  EXPECT_EQ("http://www.google.com/", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithInvalidParsedURL) {
  NSURL* url = [NSURL URLWithString:@"http:google.com:foo"];
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil]);

  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithHttpsParsedURL) {
  NSURL* url = [NSURL URLWithString:@"chromiums://www.google.com"];
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil]);

  EXPECT_EQ("https://www.google.com/", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithXCallbackURL) {
  NSURL* url =
      [NSURL URLWithString:@"chromium-x-callback://x-callback-url/open?"
                            "url=https://www.google.com"];
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil]);
  EXPECT_EQ("https://www.google.com/", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithXCallbackURLAndExtraParams) {
  NSURL* url =
      [NSURL URLWithString:@"chromium-x-callback://x-callback-url/open?"
                            "url=https://www.google.com&"
                            "x-success=http://success"];
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil]);
  EXPECT_EQ("https://www.google.com/", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithMalformedXCallbackURL) {
  NSURL* url = [NSURL
      URLWithString:@"chromium-x-callback://x-callback-url/open?url=foobar&"
                     "x-source=myapp&x-success=http://success"];
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters
          newChromeAppStartupParametersWithURL:url
                         fromSourceApplication:@"com.myapp"]);
  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithJavascriptURLInXCallbackURL) {
  NSURL* url = [NSURL
      URLWithString:
          @"chromium-x-callback://x-callback-url/open?url="
           "javascript:window.open()&x-source=myapp&x-success=http://success"];
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters
          newChromeAppStartupParametersWithURL:url
                         fromSourceApplication:@"com.myapp"]);
  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithChromeURLInXCallbackURL) {
  NSURL* url =
      [NSURL URLWithString:@"chromium-x-callback://x-callback-url/open?url="
                            "chrome:passwords"];
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters
          newChromeAppStartupParametersWithURL:url
                         fromSourceApplication:@"com.myapp"]);
  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, ParseURLWithFileParsedURL) {
  NSURL* url = [NSURL URLWithString:@"file://localhost/path/to/file.pdf"];
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters newChromeAppStartupParametersWithURL:url
                                                 fromSourceApplication:nil]);

  std::string expectedUrlString = base::StringPrintf(
      "%s://%s/file.pdf", kChromeUIScheme, kChromeUIExternalFileHost);

  EXPECT_EQ(expectedUrlString, [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupVoiceSearch) {
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters
          newAppStartupParametersForCommand:@"voicesearch"
                              withParameter:nil
                                    withURL:nil
                      fromSourceApplication:nil
                fromSecureSourceApplication:nil]);

  std::string expectedUrlString =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(expectedUrlString, [params externalURL].spec());
  EXPECT_TRUE([params launchVoiceSearch]);
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupNewTab) {
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters newAppStartupParametersForCommand:@"newtab"
                                                      withParameter:nil
                                                            withURL:nil
                                              fromSourceApplication:nil
                                        fromSecureSourceApplication:nil]);
  std::string expectedUrlString =
      base::StringPrintf("%s://%s/", kChromeUIScheme, kChromeUINewTabHost);

  EXPECT_EQ(expectedUrlString, [params externalURL].spec());
  EXPECT_FALSE([params launchVoiceSearch]);
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupOpenURL) {
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters
          newAppStartupParametersForCommand:@"openurl"
                              withParameter:@"http://foo/bar"
                                    withURL:nil
                      fromSourceApplication:nil
                fromSecureSourceApplication:nil]);

  EXPECT_EQ("http://foo/bar", [params externalURL].spec());
}

TEST_F(AppStartupParametersTest, ParseURLWithAppGroupGarbage) {
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters newAppStartupParametersForCommand:@"garbage"
                                                      withParameter:nil
                                                            withURL:nil
                                              fromSourceApplication:nil
                                        fromSecureSourceApplication:nil]);
  EXPECT_FALSE(params);
}

TEST_F(AppStartupParametersTest, FirstRunExternalLaunchSource) {
  // Key at the beginning of query string.
  CheckLaunchSourceForURL(
      first_run::LAUNCH_BY_SMARTAPPBANNER,
      @"http://www.google.com/search?safarisab=1&query=pony");
  // Key at the end of query string.
  CheckLaunchSourceForURL(
      first_run::LAUNCH_BY_SMARTAPPBANNER,
      @"http://www.google.com/search?query=pony&safarisab=1");
  // Key in the middle of query string.
  CheckLaunchSourceForURL(
      first_run::LAUNCH_BY_SMARTAPPBANNER,
      @"http://www.google.com/search?query=pony&safarisab=1&hl=en");
  // Key without '=' sign at the beginning, end, and middle of query string.
  CheckLaunchSourceForURL(first_run::LAUNCH_BY_SMARTAPPBANNER,
                          @"http://www.google.com/search?safarisab&query=pony");
  CheckLaunchSourceForURL(first_run::LAUNCH_BY_SMARTAPPBANNER,
                          @"http://www.google.com/search?query=pony&safarisab");
  CheckLaunchSourceForURL(
      first_run::LAUNCH_BY_SMARTAPPBANNER,
      @"http://www.google.com/search?query=pony&safarisab&hl=en");
  // No query string in URL.
  CheckLaunchSourceForURL(first_run::LAUNCH_BY_MOBILESAFARI,
                          @"http://www.google.com/");
  CheckLaunchSourceForURL(first_run::LAUNCH_BY_MOBILESAFARI,
                          @"http://www.google.com/safarisab/foo/bar");
  // Key not present in query string.
  CheckLaunchSourceForURL(first_run::LAUNCH_BY_MOBILESAFARI,
                          @"http://www.google.com/search?query=pony");
  // Key is a substring of some other string.
  CheckLaunchSourceForURL(
      first_run::LAUNCH_BY_MOBILESAFARI,
      @"http://www.google.com/search?query=pony&safarisabcdefg=1");
  CheckLaunchSourceForURL(
      first_run::LAUNCH_BY_MOBILESAFARI,
      @"http://www.google.com/search?query=pony&notsafarisab=1&abc=def");
}

}  // namespace
