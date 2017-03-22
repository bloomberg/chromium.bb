// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/showcase/test/showcase_matchers.h"
#import "ios/showcase/test/showcase_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for the tab container view controller.
@interface SCTabTestCase : ShowcaseTestCase
@end

@implementation SCTabTestCase

// Tests launching TopToolbarTabViewController.
- (void)testLaunchWithTopToolbar {
  [[EarlGrey selectElementWithMatcher:grey_text(@"TopToolbarTabViewController")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:showcase_matchers::FirstLevelBackButton()]
      performAction:grey_tap()];
}

// Tests launching BottomToolbarTabViewController.
- (void)testLaunchWithBottomToolbar {
  [[EarlGrey
      selectElementWithMatcher:grey_text(@"BottomToolbarTabViewController")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:showcase_matchers::FirstLevelBackButton()]
      performAction:grey_tap()];
}

@end
