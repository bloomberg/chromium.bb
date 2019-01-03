// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <TestLib/EarlGreyImpl/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/test/earl_grey2/chrome_earl_grey_edo.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test case to verify that EarlGrey tests can be launched and perform basic
// UI interactions.
@interface Eg2TestCase : XCTestCase
@end

@implementation Eg2TestCase

- (void)setUp {
  [super setUp];
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    XCUIApplication* application = [[XCUIApplication alloc] init];
    [application launch];
  });
}

// Tests that the tools menu is tappable.
- (void)testTapToolsMenu {
  id<GREYMatcher> toolsMenuButtonID =
      grey_allOf(grey_accessibilityID(@"kToolbarToolsMenuButtonIdentifier"),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:toolsMenuButtonID]
      performAction:grey_tap()];
}

// Tests that a tab can be opened.
- (void)testOpenTab {
  // Open tools menu.
  // TODO(crbug.com/917114): Calling the string directly is temporary while we
  // roll out a solution to access constants across the code base for EG2.
  id<GREYMatcher> toolsMenuButtonID =
      grey_allOf(grey_accessibilityID(@"kToolbarToolsMenuButtonIdentifier"),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:toolsMenuButtonID]
      performAction:grey_tap()];

  // Open new tab.
  // TODO(crbug.com/917114): Calling the string directly is temporary while we
  // roll out a solution to access constants across the code base for EG2.
  id<GREYMatcher> newTabButtonMatcher =
      grey_accessibilityID(@"kToolsMenuNewTabId");
  [[EarlGrey selectElementWithMatcher:newTabButtonMatcher]
      performAction:grey_tap()];

  // Get tab count.
  NSUInteger tabCount =
      [[GREYHostApplicationDistantObject sharedInstance] GetMainTabCount];
  GREYAssertEqual(2, tabCount, @"Expected 2 tabs.");
}
@end
