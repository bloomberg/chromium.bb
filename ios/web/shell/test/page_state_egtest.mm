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
const CGFloat kOffset1 = 20.0f;
const CGFloat kOffset2 = 40.0f;

// Waits for the web view scroll view is scrolled to |y_offset|.
void WaitForOffset(CGFloat y_offset) {
  CGPoint offset = CGPointMake(0.0, y_offset);
  NSString* content_offset_string = NSStringFromCGPoint(offset);
  NSString* name =
      [NSString stringWithFormat:@"Wait for scroll view to scroll to %@.",
                                 content_offset_string];
  GREYCondition* condition = [GREYCondition
      conditionWithName:name
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey
                        selectElementWithMatcher:web::WebViewScrollView()]
                        assertWithMatcher:grey_scrollViewContentOffset(offset)
                                    error:&error];
                    return (error == nil);
                  }];
  NSString* error_text =
      [NSString stringWithFormat:@"Scroll view did not scroll to %@",
                                 content_offset_string];
  GREYAssert([condition waitWithTimeout:10], error_text);
}

}  // namespace

using web::test::HttpServer;

// Page state test cases for the web shell.
@interface PageStateTestCase : ShellBaseTestCase
@end

@implementation PageStateTestCase

// Tests that page scroll position of a page is restored upon returning to the
// page via the back/forward buttons.
- (void)testScrollPositionRestoring {
  web::test::SetUpFileBasedHttpServer();

  // Load first URL which is a long page.
  [ShellEarlGrey loadURL:HttpServer::MakeUrl(kLongPage1)];

  // Scroll the first page and verify the offset.
  [[EarlGrey selectElementWithMatcher:web::WebViewScrollView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, kOffset1)];
  [[EarlGrey selectElementWithMatcher:web::WebViewScrollView()]
      assertWithMatcher:grey_scrollViewContentOffset(CGPointMake(0, kOffset1))];

  // Load second URL, which is also a long page.
  [ShellEarlGrey loadURL:HttpServer::MakeUrl(kLongPage2)];

  // Scroll the second page and verify the offset.
  [[EarlGrey selectElementWithMatcher:web::WebViewScrollView()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, kOffset2)];
  [[EarlGrey selectElementWithMatcher:web::WebViewScrollView()]
      assertWithMatcher:grey_scrollViewContentOffset(CGPointMake(0, kOffset2))];

  // Go back and verify that the first page offset has been restored.
  [[EarlGrey selectElementWithMatcher:web::BackButton()]
      performAction:grey_tap()];
  WaitForOffset(kOffset1);

  // Go forward and verify that the second page offset has been restored.
  [[EarlGrey selectElementWithMatcher:web::ForwardButton()]
      performAction:grey_tap()];
  WaitForOffset(kOffset2);
}

@end
