// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_promo_view_earlgrey_utils.h"

#import <EarlGrey/EarlGrey.h>

#import "ios/chrome/test/earl_grey/chrome_matchers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;

@implementation SignPromoViewEarlgreyUtils

+ (void)checkSigninPromoVisibleWithMode:(SigninPromoViewMode)mode {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSigninPromoViewId),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  switch (mode) {
    case SigninPromoViewModeColdState:
      [[EarlGrey
          selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                              grey_sufficientlyVisible(), nil)]
          assertWithMatcher:grey_nil()];
      break;
    case SigninPromoViewModeWarmState:
      [[EarlGrey
          selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                              grey_sufficientlyVisible(), nil)]
          assertWithMatcher:grey_notNil()];
      break;
  }
}

+ (void)checkSigninPromoNotVisible {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSigninPromoViewId),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

@end
