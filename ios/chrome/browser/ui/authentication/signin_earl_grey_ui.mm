// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"

#import <EarlGrey/EarlGrey.h>

#include "components/unified_consent/feature.h"
#include "ios/chrome/browser/ui/authentication/signin_confirmation_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_cell.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_picker_view.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::AccountConsistencyConfirmationOkButton;
using chrome_test_util::AccountConsistencySetupSigninButton;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsDoneButton;

@implementation SigninEarlGreyUI

+ (void)signinWithIdentity:(ChromeIdentity*)identity {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SecondarySignInButton()];
  if (base::FeatureList::IsEnabled(unified_consent::kUnifiedConsent)) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kIdentityPickerViewIdentifier)]
        performAction:grey_tap()];
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_accessibilityID(identity.userEmail),
                                     grey_kindOfClass(
                                         [IdentityChooserCell class]),
                                     grey_sufficientlyVisible(), nil)]
        performAction:grey_tap()];
  } else {
    [self selectIdentityWithEmail:identity.userEmail];
  }
  [self confirmSigninConfirmationDialog];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGreyUtils assertSignedInWithIdentity:identity];
}

+ (void)selectIdentityWithEmail:(NSString*)userEmail {
  // Sign in to |userEmail|.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(userEmail)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AccountConsistencySetupSigninButton()]
      performAction:grey_tap()];
}

+ (void)confirmSigninConfirmationDialog {
  // Confirm sign in. "More" button is shown on short devices (e.g. iPhone 5s,
  // iPhone SE), so needs to scroll first to dismiss the "More" button before
  // taping on "OK".
  // Cannot directly scroll on |kSignInConfirmationCollectionViewId| because it
  // is a MDC collection view, not a UICollectionView, so itself is not
  // scrollable.
  // Wait until the sync confirmation is displayed.
  id<GREYMatcher> signinUICollectionViewMatcher = nil;
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  if (base::FeatureList::IsEnabled(unified_consent::kUnifiedConsent)) {
    signinUICollectionViewMatcher =
        grey_accessibilityID(kUnifiedConsentScrollViewIdentifier);
  } else {
    signinUICollectionViewMatcher = grey_allOf(
        grey_ancestor(
            grey_accessibilityID(kSigninConfirmationCollectionViewId)),
        grey_kindOfClass([UICollectionView class]), nil);
  }
  [[EarlGrey selectElementWithMatcher:signinUICollectionViewMatcher]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [[EarlGrey selectElementWithMatcher:AccountConsistencyConfirmationOkButton()]
      performAction:grey_tap()];
}

+ (void)checkSigninPromoVisibleWithMode:(SigninPromoViewMode)mode {
  [self checkSigninPromoVisibleWithMode:mode closeButton:YES];
}

+ (void)checkSigninPromoVisibleWithMode:(SigninPromoViewMode)mode
                            closeButton:(BOOL)closeButton {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
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
  if (closeButton) {
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                kSigninPromoCloseButtonId),
                                            grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_notNil()];
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
