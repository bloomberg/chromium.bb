// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/chrome/browser/ui/payments/payment_request_picker_view_controller.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::showcase_utils::Open;
using ::showcase_utils::Close;

// Returns the GREYMatcher for the section with the given title.
id<GREYMatcher> SectionWithTitle(NSString* title) {
  return grey_allOf(grey_text(title), grey_kindOfClass([UILabel class]),
                    grey_sufficientlyVisible(), nil);
}

// Returns the GREYMatcher for the picker row with the given label. |selected|
// states whether or not the row must be selected.
id<GREYMatcher> RowWithLabel(NSString* label, BOOL selected) {
  id<GREYMatcher> matcher = grey_allOf(
      grey_ancestor(
          grey_accessibilityID(kPaymentRequestPickerRowAccessibilityID)),
      grey_text(label), grey_kindOfClass([UILabel class]),
      grey_sufficientlyVisible(), nil);

  if (selected) {
    return grey_allOf(
        matcher,
        grey_ancestor(grey_accessibilityTrait(UIAccessibilityTraitSelected)),
        nil);
  }
  return matcher;
}

// Returns the GREYMatcher for the search bar's cancel button.
id<GREYMatcher> CancelButton() {
  return grey_allOf(grey_accessibilityLabel(@"Cancel"),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

// Returns the GREYMatcher for the UIAlertView's message displayed for a call
// that notifies the delegate of a selection.
id<GREYMatcher> UIAlertViewMessageForDelegateCallWithArgument(
    NSString* argument) {
  return grey_allOf(
      grey_text(
          [NSString stringWithFormat:
                        @"paymentRequestPickerViewController:"
                        @"kPaymentRequestPickerViewControllerAccessibilityID "
                        @"didSelectRow:%@",
                        argument]),
      grey_sufficientlyVisible(), nil);
}

}  // namespace

// Tests for the payment request picker view controller.
@interface SCPaymentsPickerTestCase : ShowcaseTestCase
@end

@implementation SCPaymentsPickerTestCase

- (void)setUp {
  [super setUp];
  Open(@"PaymentRequestPickerViewController");
}

- (void)tearDown {
  Close();
  [super tearDown];
}

// Tests if all the expected rows and sections are present and the expected row
// is selected.
- (void)testVerifyRowsAndSection {
  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"B")]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Belgium", NO)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Brazil", NO)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"C")]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Canada", NO)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Chile", NO)]
      assertWithMatcher:grey_notNil()];

  // 'China' is selected.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"China", YES)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"E")]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"España", NO)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"M")]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"México", NO)]
      assertWithMatcher:grey_notNil()];
}

// Tests if filtering works.
- (void)testVerifyFiltering {
  // Type 'c' in the search bar.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kPaymentRequestPickerSearchBarAccessibilityID)]
      performAction:grey_typeText(@"c")];

  // Section 'B' should not be visible.
  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"B")]
      assertWithMatcher:grey_nil()];

  // 'Belgium' should not be visible.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Belgium", NO)]
      assertWithMatcher:grey_nil()];

  // 'Brazil' should not be visible.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Brazil", NO)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"C")]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Canada", NO)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Chile", NO)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"China", YES)]
      assertWithMatcher:grey_notNil()];

  // Section 'E' should not be visible.
  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"E")]
      assertWithMatcher:grey_nil()];

  // 'España' should not be visible.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"España", NO)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"M")]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"México", NO)]
      assertWithMatcher:grey_notNil()];

  // Type 'hi' in the search bar. So far we have typed "chi".
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kPaymentRequestPickerSearchBarAccessibilityID)]
      performAction:grey_typeText(@"hi")];

  // Section 'B' should not be visible.
  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"B")]
      assertWithMatcher:grey_nil()];

  // 'Belgium' should not be visible.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Belgium", NO)]
      assertWithMatcher:grey_nil()];

  // 'Brazil' should not be visible.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Brazil", NO)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"C")]
      assertWithMatcher:grey_notNil()];

  // 'Canada' should not be visible.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Canada", NO)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Chile", NO)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"China", YES)]
      assertWithMatcher:grey_notNil()];

  // Section 'E' should not be visible.
  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"E")]
      assertWithMatcher:grey_nil()];

  // 'España' should not be visible.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"España", NO)]
      assertWithMatcher:grey_nil()];

  // Section 'M' should not be visible.
  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"M")]
      assertWithMatcher:grey_nil()];

  // 'México' should not be visible.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"México", NO)]
      assertWithMatcher:grey_nil()];

  // Type 'l' in the search bar. So far we have typed "chil".
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kPaymentRequestPickerSearchBarAccessibilityID)]
      performAction:grey_typeText(@"l")];

  // Section 'B' should not be visible.
  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"B")]
      assertWithMatcher:grey_nil()];

  // 'Belgium' should not be visible.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Belgium", NO)]
      assertWithMatcher:grey_nil()];

  // 'Brazil' should not be visible.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Brazil", NO)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"C")]
      assertWithMatcher:grey_notNil()];

  // 'Canada' should not be visible.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Canada", NO)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Chile", NO)]
      assertWithMatcher:grey_notNil()];

  // 'China' should not be visible.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"China", YES)]
      assertWithMatcher:grey_nil()];

  // Section 'E' should not be visible.
  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"E")]
      assertWithMatcher:grey_nil()];

  // 'España' should not be visible.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"España", NO)]
      assertWithMatcher:grey_nil()];

  // Section 'M' should not be visible.
  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"M")]
      assertWithMatcher:grey_nil()];

  // 'México' should not be visible.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"México", NO)]
      assertWithMatcher:grey_nil()];

  // Cancel filtering the text in the search bar.
  [[EarlGrey selectElementWithMatcher:CancelButton()] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"B")]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Belgium", NO)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Brazil", NO)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"C")]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Canada", NO)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Chile", NO)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"China", YES)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"E")]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"España", NO)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SectionWithTitle(@"M")]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"México", NO)]
      assertWithMatcher:grey_notNil()];
}

// Tests that tapping a row should make it the selected row.
- (void)testVerifySelection {
  // 'China' is selected.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"China", YES)]
      assertWithMatcher:grey_notNil()];

  // 'Canada' is not selected. Tap it.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Canada", NO)]
      performAction:grey_tap()];

  // Confirm the delegate is informed.
  [[EarlGrey
      selectElementWithMatcher:UIAlertViewMessageForDelegateCallWithArgument(
                                   @"Label: Canada, Value: CAN")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];

  // 'China' is not selected anymore.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"China", NO)]
      assertWithMatcher:grey_notNil()];

  // Now 'Canada' is selected. Tap it again.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Canada", YES)]
      performAction:grey_tap()];

  // Confirm the delegate is informed.
  [[EarlGrey
      selectElementWithMatcher:UIAlertViewMessageForDelegateCallWithArgument(
                                   @"Label: Canada, Value: CAN")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];

  // 'China' is still not selected.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"China", NO)]
      assertWithMatcher:grey_notNil()];

  // 'Canada' is still selected.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Canada", YES)]
      assertWithMatcher:grey_notNil()];
}

@end
