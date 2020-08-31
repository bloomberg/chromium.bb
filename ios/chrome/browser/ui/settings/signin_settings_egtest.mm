// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/signin_settings_app_interface.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(SigninSettingsAppInterface);
#pragma clang diagnostic pop
#endif  // defined(CHROME_EARL_GREY_2)

using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;

@interface SigninSettingsTestCase : ChromeTestCase
@end

@implementation SigninSettingsTestCase

// Tests the primary button with a cold state.
- (void)testSignInPromoWithColdStateUsingPrimaryButton {
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];

  // Cancel the sign-in operation.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSkipSigninAccessibilityIdentifier)]
      performAction:grey_tap()];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];
}

// Tests signing in, using the primary button with a warm state.
- (void)testSignInPromoWithWarmStateUsingPrimaryButton {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [SigninEarlGreyUI confirmSigninConfirmationDialog];

  // User signed in.
  [SigninEarlGreyUtils checkSignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI checkSigninPromoNotVisible];
  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      assertWithMatcher:grey_interactable()];
}

// Tests signing in, using the secondary button with a warm state.
- (void)testSignInPromoWithWarmStateUsingSecondaryButton {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [SigninEarlGreyUI selectIdentityWithEmail:fakeIdentity.userEmail];
  [SigninEarlGreyUI confirmSigninConfirmationDialog];

  // User signed in.
  [SigninEarlGreyUtils checkSignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI checkSigninPromoNotVisible];
  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      assertWithMatcher:grey_interactable()];
}

// Tests that the sign-in promo should not be shown after been shown 5 times.
- (void)testAutomaticSigninPromoDismiss {
  const int displayedCount = 19;
  [SigninSettingsAppInterface
      setSettingsSigninPromoDisplayedCount:displayedCount];
  [ChromeEarlGreyUI openSettingsMenu];
  // Check the sign-in promo view is visible.
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];
  // Check the sign-in promo will not be shown anymore.
  int newDisplayedCount =
      [SigninSettingsAppInterface settingsSigninPromoDisplayedCount];
  GREYAssertEqual(displayedCount + 1, newDisplayedCount,
                  @"Should have incremented the display count");
  // Close the settings menu and open it again.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI openSettingsMenu];
  // Check that the sign-in promo is not visible anymore.
  [SigninEarlGreyUI checkSigninPromoNotVisible];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSettingsSignInCellId),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

- (void)testDissmissSigninPromo {
  [ChromeEarlGreyUI openSettingsMenu];
  // Check the sign-in promo view is visible.
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];
  // Tap on dismiss button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSigninPromoCloseButtonId),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Check that the sign-in promo is not visible anymore.
  [SigninEarlGreyUI checkSigninPromoNotVisible];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSettingsSignInCellId),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

@end
