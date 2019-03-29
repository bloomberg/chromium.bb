// Copyright 2019 The Chromium Authors. All rights reserved.
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

// Tests for the Infobar Banner.
@interface SCInfobarBannerTestCase : ShowcaseTestCase
@end

@implementation SCInfobarBannerTestCase

- (void)setUp {
  [super setUp];
  Open(@"Infobar Banner");
}

- (void)tearDown {
  Close();
  [super tearDown];
}

// Tests that the InfobarBanner is presented.
- (void)testInfobarBannerWasPresented {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Test Infobar")]
      assertWithMatcher:grey_notNil()];
}

@end
