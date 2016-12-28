// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <CoreGraphics/CoreGraphics.h>
#import <EarlGrey/EarlGrey.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#import "ios/web/shell/test/earl_grey/shell_base_test_case.h"
#import "ios/web/shell/test/earl_grey/shell_earl_grey.h"
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

using web::test::HttpServer;

// Page state test cases for the web shell.
@interface PageStateTestCase : ShellBaseTestCase
@end

@implementation PageStateTestCase

// TODO(crbug.com/675015): Re-enable this test on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testScrollPositionRestoring testScrollPositionRestoring
#else
#define MAYBE_testScrollPositionRestoring FLAKY_testScrollPositionRestoring
#endif
// Tests that page scroll position of a page is restored upon returning to the
// page via the back/forward buttons.
- (void)MAYBE_testScrollPositionRestoring {
  // TODO(crbug.com/670700): Re-enable this test.
  if (!base::ios::IsRunningOnIOS10OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on pre-iOS 10");
  }
  web::test::SetUpFileBasedHttpServer();

  // Load first URL which is a long page.
  [ShellEarlGrey loadURL:HttpServer::MakeUrl(kLongPage1)];

  // Scroll the first page and verify the offset.
  [[EarlGrey selectElementWithMatcher:web::webViewScrollView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, kScrollOffset1)];
  [[EarlGrey selectElementWithMatcher:web::webViewScrollView()]
      assertWithMatcher:contentOffset(CGPointMake(0, kScrollOffset1))];

  // Load second URL, which is also a long page.
  [ShellEarlGrey loadURL:HttpServer::MakeUrl(kLongPage2)];

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
