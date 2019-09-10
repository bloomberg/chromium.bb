// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/features.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::AddPaymentMethodButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::PaymentMethodsButton;

namespace {

// Matcher for the 'Name on Card' field in the add credit card view.
id<GREYMatcher> NameOnCardField() {
  return grey_accessibilityLabel(
      l10n_util::GetNSStringWithFixup(IDS_IOS_AUTOFILL_CARDHOLDER));
}

// Matcher for the 'Card Number' field in the add credit card view.
id<GREYMatcher> CardNumberField() {
  return grey_accessibilityLabel(
      l10n_util::GetNSStringWithFixup(IDS_IOS_AUTOFILL_CARD_NUMBER));
}

// Matcher for the 'Month of Expiry' field in the add credit card view.
id<GREYMatcher> MonthOfExpiryField() {
  return grey_accessibilityLabel(
      l10n_util::GetNSStringWithFixup(IDS_IOS_AUTOFILL_EXP_MONTH));
}

// Matcher for the 'Year of Expiry' field in the add credit card view.
id<GREYMatcher> YearOfExpiryField() {
  return grey_accessibilityLabel(
      l10n_util::GetNSStringWithFixup(IDS_IOS_AUTOFILL_EXP_YEAR));
}

// Matcher for the use camera button in the add credit card view.
id<GREYMatcher> UseCameraButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_AUTOFILL_ADD_CREDIT_CARD_OPEN_CAMERA_BUTTON_LABEL);
}

}  // namespace

// Tests for Settings Autofill add credit cards section.
@interface AutofillAddCreditCardTestCase : ChromeTestCase
@end

@implementation AutofillAddCreditCardTestCase

- (void)setUp {
  [super setUp];
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithFeaturesEnabled:{kSettingsAddPaymentMethod,
                                            kCreditCardScanner}
                                  disabled:{}
                              forceRestart:NO];
  GREYAssertTrue([ChromeEarlGrey isSettingsAddPaymentMethodEnabled],
                 @"SettingsAddPaymentMethod should be enabled");
  GREYAssertTrue([ChromeEarlGrey isCreditCardScannerEnabled],
                 @"CreditCardScanner should be enabled");
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PaymentMethodsButton()];
  [[EarlGrey selectElementWithMatcher:AddPaymentMethodButton()]
      performAction:grey_tap()];
}

#pragma mark - Test that all fields on the 'Add Credit Card' screen appear

// Tests that 'Name on Card' field appears on screen.
- (void)testNameOnCardFieldIsPresent {
  [[EarlGrey selectElementWithMatcher:NameOnCardField()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that 'Card Number' field appears on screen.
- (void)testCardNumberFieldIsPresent {
  [[EarlGrey selectElementWithMatcher:CardNumberField()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that 'Month of Expiry' field appears on screen.
- (void)testMonthOfExpiryFieldIsPresent {
  [[EarlGrey selectElementWithMatcher:MonthOfExpiryField()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that 'Year of Expiry' field appears on screen.
- (void)testYearOfExpiryFieldIsPresent {
  [[EarlGrey selectElementWithMatcher:YearOfExpiryField()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that 'Use Camera' button appears on screen only for iOS 13.
- (void)testUseCameraButtonIsPresent {
  if (@available(iOS 13, *)) {
    [[EarlGrey selectElementWithMatcher:UseCameraButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
  } else {
    [[EarlGrey selectElementWithMatcher:UseCameraButton()]
        assertWithMatcher:grey_nil()];
  }
}

@end
