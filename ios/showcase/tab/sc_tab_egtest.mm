// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for the tab container view controller.
@interface SCTabTestCase : ShowcaseTestCase
@end

@implementation SCTabTestCase

// Tests launching TopToolbarTabViewController.
// TODO(crbug.com/710662): re-enable this test.
- (void)FLAKY_testLaunchWithTopToolbar {
  showcase_utils::Open(@"TopToolbarTabViewController");
  showcase_utils::Close();
}

// Tests launching BottomToolbarTabViewController.
// TODO(crbug.com/710662): re-enable this test.
- (void)FLAKY_testLaunchWithBottomToolbar {
  showcase_utils::Open(@"BottomToolbarTabViewController");
  showcase_utils::Close();
}

@end
