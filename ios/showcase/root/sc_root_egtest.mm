// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for the root container view controller.
@interface SCRootTestCase : ShowcaseTestCase
@end

@implementation SCRootTestCase

// Tests launching RootContainerViewController.
- (void)testLaunch {
  showcase_utils::Open(@"RootContainerViewController");
  showcase_utils::Close();
}

@end
