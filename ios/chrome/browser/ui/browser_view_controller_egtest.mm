// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#import <EarlGrey/EarlGrey.h>
#import <WebKit/WebKit.h>
#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#include "ios/web/public/test/response_providers/html_response_provider.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// This test suite only tests javascript in the omnibox. Nothing to do with BVC
// really, the name is a bit misleading.
@interface BrowserViewControllerTestCase : ChromeTestCase
@end

@implementation BrowserViewControllerTestCase

// Tests that evaluating JavaScript in the omnibox (e.g, a bookmarklet) works.
- (void)testJavaScriptInOmnibox {
  // TODO(crbug.com/640220): Keyboard entry inside the omnibox fails only on
  // iPad
  // running iOS X.
  if (IsIPadIdiom() && base::ios::IsRunningOnIOS10OrLater())
    return;

  // Preps the http server with two URLs serving content.
  std::map<GURL, std::string> responses;
  const GURL startURL = web::test::HttpServer::MakeUrl("http://origin");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");
  responses[startURL] = "Start";
  responses[destinationURL] = "You've arrived!";
  web::test::SetUpSimpleHttpServer(responses);

  // Just load the first URL.
  [ChromeEarlGrey loadURL:startURL];

  // Waits for the page to load and check it is the expected content.
  id<GREYMatcher> responseMatcher =
      chrome_test_util::WebViewContainingText(responses[startURL]);
  [[EarlGrey selectElementWithMatcher:responseMatcher]
      assertWithMatcher:grey_notNil()];

  // In the omnibox, the URL should be present, without the http:// prefix.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(startURL.GetContent())];

  // Types some javascript in the omnibox to trigger a navigation.
  NSString* script =
      [NSString stringWithFormat:@"javascript:location.href='%s'\n",
                                 destinationURL.spec().c_str()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(script)];

  // In the omnibox, the new URL should be present, without the http:// prefix.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(
                            destinationURL.GetContent())];

  // Verifies that the navigation to the destination page happened.
  GREYAssertEqual(destinationURL,
                  chrome_test_util::GetCurrentWebState()->GetVisibleURL(),
                  @"Did not navigate to the destination url.");

  // Verifies that the destination page is shown.
  id<GREYMatcher> navigationMatcher =
      chrome_test_util::WebViewContainingText(responses[destinationURL]);
  [[EarlGrey selectElementWithMatcher:grey_kindOfClass([WKWebView class])]
      assertWithMatcher:navigationMatcher];
}

@end
