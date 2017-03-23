// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for the suggestions view controller.
@interface SCContentSuggestionsTestCase : ShowcaseTestCase
@end

@implementation SCContentSuggestionsTestCase

// Tests launching ContentSuggestionsViewController.
- (void)testLaunch {
  showcase_utils::Open(@"ContentSuggestionsViewController");
  showcase_utils::Close();
}

@end
