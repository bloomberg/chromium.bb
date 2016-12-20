// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/showcase/test/showcase_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for the tab grid view controller.
@interface SCTabGridTestCase : ShowcaseTestCase
@end

@implementation SCTabGridTestCase

// Tests launching TabGridViewController and tapping a cell.
- (void)testLaunchAndTappingCell {
  [[EarlGrey selectElementWithMatcher:grey_text(@"TabGridViewController")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_text(@"Tab 0")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_text(@"TabGridActionDelegate")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_text(@"Done")]
      performAction:grey_tap()];
}

@end
