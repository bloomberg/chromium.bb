// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for the tab grid view controller.
@interface SCTabGridTestCase : ShowcaseTestCase
@end

@implementation SCTabGridTestCase

// Tests launching TabGridViewController and tapping a cell.
// TODO(crbug.com/710662): re-enable this test.
- (void)FLAKY_testLaunchAndTappingCell {
  showcase_utils::Open(@"TabGridViewController");
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Tab 0_button")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_text(@"TabGridCommands")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];
  showcase_utils::Close();
}

@end
