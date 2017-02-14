// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/showcase/test/showcase_test_case.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for the tab grid view controller.
@interface SCTabGridTestCase : ShowcaseTestCase
@end

@implementation SCTabGridTestCase

// Tests launching TabGridViewController and tapping a cell.
- (void)testLaunchAndTappingCell {
// TODO(crbug.com/687865): enable the test. It was flaky on device.
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(
      @"Disabled for devices because it is flaky on iPhone");
#endif
  [[EarlGrey selectElementWithMatcher:grey_text(@"TabGridViewController")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Tab 0_button")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_text(@"TabCommands")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];
}

@end
