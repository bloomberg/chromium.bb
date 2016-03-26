// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import <EarlGrey/EarlGrey.h>

#import "ios/web/shell/view_controller.h"

@interface CRWWebShellNavigationTest : XCTestCase

@end

@implementation CRWWebShellNavigationTest

// Sample test to load a live URL, go back and then forward.
- (void)testExternalURLBackAndForward {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   kWebShellAddressFieldAccessibilityLabel)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Clear text")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   kWebShellAddressFieldAccessibilityLabel)]
      performAction:grey_typeText(@"http://browsingtest.appspot.com\n")];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   kWebShellBackButtonAccessibilityLabel)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   kWebShellForwardButtonAccessibilityLabel)]
      performAction:grey_tap()];
}

// Sample test to load a live URL, go back, forward, and then back again.
- (void)testExternalURLBackAndForwardAndBack {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   kWebShellAddressFieldAccessibilityLabel)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Clear text")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   kWebShellAddressFieldAccessibilityLabel)]
      performAction:grey_typeText(@"http://browsingtest.appspot.com\n")];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   kWebShellBackButtonAccessibilityLabel)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   kWebShellForwardButtonAccessibilityLabel)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   kWebShellBackButtonAccessibilityLabel)]
      performAction:grey_tap()];
}
@end
