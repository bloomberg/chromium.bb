// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/payments/cells/price_item.h"
#import "ios/chrome/browser/ui/payments/payment_request_view_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server/http_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using payment_request_util::GetAddressNotificationLabelFromAutofillProfile;
using payment_request_util::GetEmailLabelFromAutofillProfile;
using payment_request_util::GetNameLabelFromAutofillProfile;
using payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
using payment_request_util::GetShippingAddressLabelFromAutofillProfile;

// Displacement for scroll action.
const CGFloat kScrollDisplacement = 100.0;
// URL of the Payment Request test page.
const char kPaymentRequestDemoPage[] =
    "http://components/test/data/payments/payment_request.html";

// Finds the shipping address cell on the Payment Summary page.
id<GREYMatcher> ShippingAddressCellMatcher(autofill::AutofillProfile* profile) {
  NSString* email_label = nil;
  NSString* notification_label = nil;

  return chrome_test_util::ButtonWithAccessibilityLabel([NSString
      stringWithFormat:@"%@, %@, %@, %@, %@",
                       GetNameLabelFromAutofillProfile(*profile),
                       GetShippingAddressLabelFromAutofillProfile(*profile),
                       GetPhoneNumberLabelFromAutofillProfile(*profile),
                       email_label, notification_label]);
}

// Finds the payment method cell on the Payment Summary page.
id<GREYMatcher> PaymentMethodCellMatcher(autofill::CreditCard* credit_card) {
  NSString* billing_address_label = nil;
  NSString* notification_label = nil;

  return chrome_test_util::ButtonWithAccessibilityLabel([NSString
      stringWithFormat:@"%@, %@, %@, %@",
                       base::SysUTF16ToNSString(
                           credit_card->NetworkAndLastFourDigits()),
                       base::SysUTF16ToNSString(credit_card->GetRawInfo(
                           autofill::CREDIT_CARD_NAME_FULL)),
                       billing_address_label, notification_label]);
}

// Finds the order summary cell on the Payment Summary page.
id<GREYMatcher> PriceCellMatcher(NSString* main_label, NSString* price_label) {
  NSString* notification_label = nil;

  return chrome_test_util::ButtonWithAccessibilityLabel(
      [NSString stringWithFormat:@"%@, %@, %@", main_label, notification_label,
                                 price_label]);
}

// Finds the shipping option cell on the Payment Summary page.
id<GREYMatcher> ShippingOptionCellMatcher(NSString* main_label,
                                          NSString* detail_label) {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      [NSString stringWithFormat:@"%@, %@", main_label, detail_label]);
}

// Finds the contact info cell on the Payment Summary page.
id<GREYMatcher> ContactInfoCellMatcher(autofill::AutofillProfile* profile) {
  NSString* address_label = nil;
  NSString* notification_label = nil;

  return chrome_test_util::ButtonWithAccessibilityLabel([NSString
      stringWithFormat:@"%@, %@, %@, %@, %@",
                       GetNameLabelFromAutofillProfile(*profile), address_label,
                       GetPhoneNumberLabelFromAutofillProfile(*profile),
                       GetEmailLabelFromAutofillProfile(*profile),
                       notification_label]);
}

}  // namespace

// Various tests for Payment Request.
@interface PaymentRequestTestCase : ChromeTestCase

@end

@implementation PaymentRequestTestCase {
  autofill::AutofillProfile _profile;
  autofill::CreditCard _creditCard1;
  autofill::CreditCard _creditCard2;
  GURL _testURL;
}

#pragma mark - XCTest.

// Set up called once before each test.
- (void)setUp {
  [super setUp];

  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  _profile = autofill::test::GetFullProfile();
  _creditCard1 = autofill::test::GetCreditCard();
  _creditCard2 = autofill::test::GetCreditCard2();

  personalDataManager->AddProfile(_profile);
  _creditCard1.set_billing_address_id(_profile.guid());
  personalDataManager->AddCreditCard(_creditCard1);
  personalDataManager->AddCreditCard(_creditCard2);

  _testURL = web::test::HttpServer::MakeUrl(kPaymentRequestDemoPage);
}

#pragma mark - Tests.

// Tests accessibility on the Payment Request summary page.
- (void)testAccessibilityOnPaymentRequestSummaryPage {
  [ChromeEarlGrey loadURL:_testURL];

  // Tap the buy button.
  chrome_test_util::TapWebViewElementWithId("buy");

  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests accessibility on the Payment Request order summary page.
- (void)testAccessibilityOnPaymentRequestOrderSummaryPage {
  [ChromeEarlGrey loadURL:_testURL];

  // Tap the buy button.
  chrome_test_util::TapWebViewElementWithId("buy");

  [[EarlGrey
      selectElementWithMatcher:PriceCellMatcher(@"Donation", @"USD $55.00")]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests accessibility on the Payment Request delivery address page.
- (void)testAccessibilityOnPaymentRequestDeliveryAddressPage {
  [ChromeEarlGrey loadURL:_testURL];

  // Tap the buy button.
  chrome_test_util::TapWebViewElementWithId("buy");

  [[EarlGrey selectElementWithMatcher:ShippingAddressCellMatcher(&_profile)]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests accessibility on the Payment Request delivery method page.
- (void)testAccessibilityOnPaymentRequestDeliveryMethodPage {
  [ChromeEarlGrey loadURL:_testURL];

  // Tap the buy button.
  chrome_test_util::TapWebViewElementWithId("buy");

  [[EarlGrey selectElementWithMatcher:ShippingOptionCellMatcher(
                                          @"Standard shipping", @"$0.00")]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests accessibility on the Payment Request payment method page.
- (void)testAccessibilityOnPaymentRequestPaymentMethodPage {
  [ChromeEarlGrey loadURL:_testURL];

  // Tap the buy button.
  chrome_test_util::TapWebViewElementWithId("buy");

  [[[EarlGrey selectElementWithMatcher:PaymentMethodCellMatcher(&_creditCard1)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollDisplacement)
      onElementWithMatcher:grey_accessibilityID(
                               kPaymentRequestCollectionViewID)]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests accessibility on the Payment Request contact info page.
- (void)testAccessibilityOnPaymentRequestContactInfoPage {
  [ChromeEarlGrey loadURL:_testURL];

  // Tap the buy button.
  chrome_test_util::TapWebViewElementWithId("buy");

  [[[EarlGrey selectElementWithMatcher:ContactInfoCellMatcher(&_profile)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollDisplacement)
      onElementWithMatcher:grey_accessibilityID(
                               kPaymentRequestCollectionViewID)]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

@end
