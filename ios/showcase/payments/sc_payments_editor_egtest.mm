// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/autofill/autofill_edit_accessory_view.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UIWindow (Hidden)
- (UIResponder*)firstResponder;
@end

namespace {
using ::showcase_utils::Open;
using ::showcase_utils::Close;

// Returns the GREYMatcher for the input accessory view's previus button.
id<GREYMatcher> InputAccessoryViewPreviousButton() {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_ACCNAME_PREVIOUS)),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_sufficientlyVisible(), nil);
}

// Returns the GREYMatcher for the input accessory view's next button.
id<GREYMatcher> InputAccessoryViewNextButton() {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_ACCNAME_NEXT)),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_sufficientlyVisible(), nil);
}

// Returns the GREYMatcher for the input accessory view's close button.
id<GREYMatcher> InputAccessoryViewCloseButton() {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_ACCNAME_CLOSE)),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_sufficientlyVisible(), nil);
}

void AssertTextFieldWithAccessibilityIDIsFirstResponder(
    NSString* accessibilityID) {
  UIResponder* firstResponder =
      [[UIApplication sharedApplication].keyWindow firstResponder];
  GREYAssertTrue([firstResponder isKindOfClass:[UITextField class]],
                 @"Expected first responder to be of kind %@, got %@.",
                 [UITextField class], [firstResponder class]);
  UITextField* textField =
      base::mac::ObjCCastStrict<UITextField>(firstResponder);
  GREYAssertTrue(
      [[textField accessibilityIdentifier] isEqualToString:accessibilityID],
      @"Expected accessibility identifier to be %@, got %@.", accessibilityID,
      [textField accessibilityIdentifier]);
}

}  // namespace

// Tests for the payment request editor view controller.
@interface SCPaymentsEditorTestCase : ShowcaseTestCase
@end

@implementation SCPaymentsEditorTestCase

- (void)setUp {
  [super setUp];
  Open(@"PaymentRequestEditViewController");
}

- (void)tearDown {
  Close();
  [super tearDown];
}

// Tests if expected labels and textfields exist and have the expected values.
- (void)testVerifyLabelsAndTextFields {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Name*")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Name_textField")]
      assertWithMatcher:grey_text(@"John Doe")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Address*")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Address_textField")]
      assertWithMatcher:grey_text(@"")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Postal Code")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Postal Code_textField")]
      assertWithMatcher:grey_text(@"")];
}

// Tests whether tapping the input accessory view's close button dismisses the
// input accessory view.
- (void)testInputAccessoryViewCloseButton {
  if (IsIPadIdiom()) {
    // TODO(crbug.com/602666): Investigate why the close button is hidden on
    // iPad.
    EARL_GREY_TEST_DISABLED(
        @"Input accessory view's close button is hidden on iPad");
  }

  // Initially, the input ​accessory view is not showing.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAutofillEditAccessoryViewAccessibilityID)]
      assertWithMatcher:grey_nil()];

  // Tap the name textfield.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Name_textField")]
      performAction:grey_tap()];

  // Assert the input accessory view's close button is enabled and tap it.
  [[[EarlGrey selectElementWithMatcher:InputAccessoryViewCloseButton()]
      assertWithMatcher:grey_enabled()] performAction:grey_tap()];

  // Tapping the input accessory view's close button should've dismissed it.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAutofillEditAccessoryViewAccessibilityID)]
      assertWithMatcher:grey_nil()];
}

// Tests whether the input accessory view navigation buttons have the correct
// states depending on the focused textfield and that they can be used to
// navigate between the textfields.
- (void)testInputAccessoryViewNavigationButtons {
  // Initially, no error message is showing.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kWarningMessageAccessibilityID)]
      assertWithMatcher:grey_nil()];

  // Tap the name textfield.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Name_textField")]
      performAction:grey_tap()];

  // Assert the name textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(@"Name_textField");

  // Assert the input accessory view's previous button is disabled.
  [[EarlGrey selectElementWithMatcher:InputAccessoryViewPreviousButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  // Assert the input accessory view's next button is enabled and tap it.
  [[[EarlGrey selectElementWithMatcher:InputAccessoryViewNextButton()]
      assertWithMatcher:grey_enabled()] performAction:grey_tap()];

  // Assert the address textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(@"Address_textField");

  // Assert the input accessory view's previous button is enabled.
  [[EarlGrey selectElementWithMatcher:InputAccessoryViewPreviousButton()]
      assertWithMatcher:grey_enabled()];
  // Assert the input accessory view's next button is enabled and tap it.
  [[[EarlGrey selectElementWithMatcher:InputAccessoryViewNextButton()]
      assertWithMatcher:grey_enabled()] performAction:grey_tap()];

  // Assert an error message is showing because the address textfield is
  // required.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kWarningMessageAccessibilityID)]
      assertWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                            IDS_PAYMENTS_FIELD_REQUIRED_VALIDATION_MESSAGE))];

  // Assert the postal code textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(@"Postal Code_textField");

  // Assert the input accessory view's next button is disabled.
  [[EarlGrey selectElementWithMatcher:InputAccessoryViewNextButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  // Assert the input accessory view's previous button is enabled and tap it.
  [[[EarlGrey selectElementWithMatcher:InputAccessoryViewPreviousButton()]
      assertWithMatcher:grey_enabled()] performAction:grey_tap()];

  // Assert the address textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(@"Address_textField");

  // Type in an address.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Address_textField")]
      performAction:grey_typeText(@"Main St")];

  // Assert the input accessory view's next button is enabled.
  [[EarlGrey selectElementWithMatcher:InputAccessoryViewNextButton()]
      assertWithMatcher:grey_enabled()];
  // Assert the input accessory view's previous button is enabled and tap it.
  [[[EarlGrey selectElementWithMatcher:InputAccessoryViewPreviousButton()]
      assertWithMatcher:grey_enabled()] performAction:grey_tap()];

  // Assert the error message disappeared because an address was typed in.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kWarningMessageAccessibilityID)]
      assertWithMatcher:grey_notVisible()];

  // Assert the name textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(@"Name_textField");

  // Assert the input accessory view's previous button is disabled.
  [[EarlGrey selectElementWithMatcher:InputAccessoryViewPreviousButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  // Assert the input accessory view's next button is enabled.
  [[EarlGrey selectElementWithMatcher:InputAccessoryViewNextButton()]
      assertWithMatcher:grey_enabled()];
}

// Tests tapping the return key on every textfield causes the next textfield to
// get focus except for the last textfield in which case causes the focus to go
// away from the textfield.
- (void)testNavigationByTappingReturn {
  // Tap the name textfield.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Name_textField")]
      performAction:grey_tap()];

  // Assert the name textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(@"Name_textField");

  // Press the return key on the name textfield.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Name_textField")]
      performAction:grey_typeText(@"\n")];

  // Assert the address textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(@"Address_textField");

  // Press the return key on the address textfield.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Address_textField")]
      performAction:grey_typeText(@"\n")];

  // Assert the postal code textfield is focused.
  AssertTextFieldWithAccessibilityIDIsFirstResponder(@"Postal Code_textField");

  // Press the return key on the postal code textfield.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Postal Code_textField")]
      performAction:grey_typeText(@"\n")];

  // Expect non of the textfields to be focused.
  UIResponder* firstResponder =
      [[UIApplication sharedApplication].keyWindow firstResponder];
  GREYAssertFalse([firstResponder isKindOfClass:[UITextField class]],
                  @"Expected first responder not to be of kind %@.",
                  [UITextField class]);
}

@end
