// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::showcase_utils::Open;
using ::showcase_utils::Close;
}

@interface TextBadgeViewTestCase : ShowcaseTestCase
@end

@implementation TextBadgeViewTestCase

// Tests that the accessibility label matches the display text.
- (void)testTextBadgeAccessibilityLabel {
  Open(@"TextBadgeView");
  Close();
}

@end
