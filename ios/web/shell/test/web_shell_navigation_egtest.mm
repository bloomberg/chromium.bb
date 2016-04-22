// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#import <XCTest/XCTest.h>

#import <EarlGrey/EarlGrey.h>

#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#include "ios/web/shell/test/navigation_test_util.h"
#import "ios/web/shell/test/shell_matchers.h"
#import "ios/web/shell/test/web_view_matchers.h"

// Navigation test cases for the web shell. These are Earl Grey integration
// tests, which are based on XCTest.
@interface CRWWebShellNavigationTest : XCTestCase

@end

@implementation CRWWebShellNavigationTest

// Set up called once for the class.
+ (void)setUp {
  [super setUp];
  [[EarlGrey selectElementWithMatcher:shell_webViewContainingText(@"Chromium")]
      assertWithMatcher:grey_notNil()];
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  server.StartOrDie();
  DCHECK(server.IsRunning());
}

// Tear down called once for the class.
+ (void)tearDown {
  [super tearDown];
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  server.Stop();
  DCHECK(!server.IsRunning());
}

// Tear down called after each test.
- (void)tearDown {
  [super tearDown];
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  server.RemoveAllResponseProviders();
}

// Tests the back and forward button after entering two URLs.
- (void)testWebScenarioBrowsingBackAndForward {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL1 = web::test::HttpServer::MakeUrl("http://firstURL");
  NSString* response1 = @"Test Page 1";
  responses[URL1] = base::SysNSStringToUTF8(response1);

  const GURL URL2 = web::test::HttpServer::MakeUrl("http://secondURL");
  NSString* response2 = @"Test Page 2";
  responses[URL2] = base::SysNSStringToUTF8(response2);

  web::test::SetUpSimpleHttpServer(responses);

  web::navigation_test_util::LoadUrl(URL1);

  [[EarlGrey selectElementWithMatcher:shell_webViewContainingText(response1)]
      assertWithMatcher:grey_notNil()];

  web::navigation_test_util::LoadUrl(URL2);

  [[EarlGrey selectElementWithMatcher:shell_webViewContainingText(response2)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:shell_backButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:shell_webViewContainingText(response1)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:shell_forwardButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:shell_webViewContainingText(response2)]
      assertWithMatcher:grey_notNil()];
}

@end
