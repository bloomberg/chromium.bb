// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"

#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TEST(SigninPromoViewTest, SecondaryButtonVisibility) {
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  view.mode = SigninPromoViewModeColdState;
  EXPECT_TRUE(view.secondaryButton.hidden);
  view.mode = SigninPromoViewModeWarmState;
  EXPECT_FALSE(view.secondaryButton.hidden);
}
