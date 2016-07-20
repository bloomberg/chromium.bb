// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <CoreGraphics/CoreGraphics.h>
#import <EarlGrey/EarlGrey.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#include "ios/web/shell/test/app/navigation_test_util.h"
#import "ios/web/shell/test/earl_grey/shell_matchers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// URLs for test pages.
const char kLongPage1[] =
    "http://ios/web/shell/test/http_server_files/tall_page.html";
const char kLongPage2[] =
    "http://ios/web/shell/test/http_server_files/tall_page.html?2";

// Test scroll offsets.
const CGFloat kScrollOffset1 = 20.0f;
const CGFloat kScrollOffset2 = 40.0f;

// Returns a matcher for asserting that element's content offset matches the
// given |offset|.
id<GREYMatcher> contentOffset(CGPoint offset) {
  MatchesBlock matches = ^BOOL(UIScrollView* element) {
    return CGPointEqualToPoint([element contentOffset], offset);
  };
  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"contentOffset"];
  };
  return grey_allOf(
      grey_kindOfClass([UIScrollView class]),
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe],
      nil);
}

}  // namespace

using web::shell_test_util::LoadUrl;
using web::test::HttpServer;
using web::webViewContainingText;

// Page state test cases for the web shell.
@interface CRWWebShellPageStateTest : XCTestCase
@end

@implementation CRWWebShellPageStateTest

// Set up called once for the class.
+ (void)setUp {
  [super setUp];
  [[EarlGrey selectElementWithMatcher:webViewContainingText("Chromium")]
      assertWithMatcher:grey_notNil()];
  HttpServer::GetSharedInstance().StartOrDie();
}

// Tear down called once for the class.
+ (void)tearDown {
  [super tearDown];
  HttpServer::GetSharedInstance().Stop();
}

// Tear down called after each test.
- (void)tearDown {
  [super tearDown];
  HttpServer::GetSharedInstance().RemoveAllResponseProviders();
}

// Tests that page scroll position of a page is restored upon returning to the
// page via the back/forward buttons.
- (void)testScrollPositionRestoring {
  web::test::SetUpFileBasedHttpServer();

  // Load first URL which is a long page.
  LoadUrl(HttpServer::MakeUrl(kLongPage1));
  // TODO(crbug.com/629116): Remove this once |LoadUrl| waits for the load
  // completion.
  [[EarlGrey selectElementWithMatcher:webViewContainingText("List of numbers")]
      assertWithMatcher:grey_notNil()];

  // Scroll the first page and verify the offset.
  [[EarlGrey selectElementWithMatcher:web::webViewScrollView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, kScrollOffset1)];
  [[EarlGrey selectElementWithMatcher:web::webViewScrollView()]
      assertWithMatcher:contentOffset(CGPointMake(0, kScrollOffset1))];

  // Load second URL, which is also a long page.
  GURL URL2 = HttpServer::MakeUrl(kLongPage2);
  LoadUrl(URL2);
  // TODO(crbug.com/629116): Remove these once |LoadUrl| waits for the load
  // completion.
  [[EarlGrey selectElementWithMatcher:web::addressFieldText(URL2.spec())]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:webViewContainingText("List of numbers")]
      assertWithMatcher:grey_notNil()];

  // Scroll the second page and verify the offset.
  [[EarlGrey selectElementWithMatcher:web::webViewScrollView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, kScrollOffset2)];
  [[EarlGrey selectElementWithMatcher:web::webViewScrollView()]
      assertWithMatcher:contentOffset(CGPointMake(0, kScrollOffset2))];

  // Go back and verify that the first page offset has been restored.
  [[EarlGrey selectElementWithMatcher:web::backButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:web::webViewScrollView()]
      assertWithMatcher:contentOffset(CGPointMake(0, kScrollOffset1))];

  // Go forward and verify that the second page offset has been restored.
  [[EarlGrey selectElementWithMatcher:web::forwardButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:web::webViewScrollView()]
      assertWithMatcher:contentOffset(CGPointMake(0, kScrollOffset2))];
}

@end
