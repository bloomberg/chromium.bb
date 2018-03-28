// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/payments/payment_request_egtest_base.h"
#import "ios/chrome/browser/ui/payments/payment_request_error_view_controller.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/http_server/http_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Constant for timeout while waiting for the show promise to settle.
const NSTimeInterval kShowPromiseTimeout = 10.0;

using chrome_test_util::ButtonWithAccessibilityLabelId;

// URLs of the test pages.
const char kShowPromisePage[] =
    "https://components/test/data/payments/"
    "payment_request_show_promise.html";

// Matcher for the PriceCell.
id<GREYMatcher> PriceCellMatcher(NSString* accessibilityLabel) {
  return grey_allOf(grey_accessibilityLabel(accessibilityLabel),
                    grey_sufficientlyVisible(), nil);
}

}  // namepsace

// Tests for the PaymentDetailsModifier.
@interface PaymentRequestShowEGTest : PaymentRequestEGTestBase

@end

@implementation PaymentRequestShowEGTest

- (void)setUp {
  [super setUp];

  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  [self addAutofillProfile:profile];

  autofill::CreditCard localCard = autofill::test::GetCreditCard();  // Visa.
  localCard.set_billing_address_id(profile.guid());
  [self addCreditCard:localCard];
}

#pragma mark - Tests

// Tests when PaymentRequest.show() is called without a promise the payment
// sheet is displayed with the payment details.
- (void)testBuyWithNoPromise {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kShowPromisePage)];

  [ChromeEarlGrey tapWebViewElementWithID:@"buyWithNoPromise"];

  // Confirm that the Payment Request UI is showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_notNil()];

  // Verify that the Buy button is enabled.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_PAYMENTS_PAY_BUTTON)]
      assertWithMatcher:grey_enabled()];

  // Verify that the total amount is displayed.
  [[EarlGrey selectElementWithMatcher:PriceCellMatcher(@"Donation, USD $1.00")]
      assertWithMatcher:grey_notNil()];
}

// Tests when PaymentRequest.show() is called with a promise, the payment sheet
// is initially displayed with a spinner and the Buy button is not enabled. Once
// the promise resolves, the payment sheet displays the payment details and the
// Buy button is enabled.
- (void)testBuyWithResolvingPromise {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kShowPromisePage)];

  // Disable EarlGrey's synchronization.
  [[GREYConfiguration sharedInstance]
          setValue:@NO
      forConfigKey:kGREYConfigKeySynchronizationEnabled];

  [ChromeEarlGrey tapWebViewElementWithID:@"buyWithResolvingPromise"];

  // Confirm that the Payment Request UI is showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_notNil()];

  // Verify that the Buy button is not enabled.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_PAYMENTS_PAY_BUTTON)]
      assertWithMatcher:grey_not(grey_enabled())];

  // Wait until the Buy button becomes enabled.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                            IDS_PAYMENTS_PAY_BUTTON)]
        assertWithMatcher:grey_enabled()
                    error:&error];
    return error == nil;
  };
  GREYAssert(
      testing::WaitUntilConditionOrTimeout(kShowPromiseTimeout, condition),
      @"Show promise failed to resolve.");

  // Verify that the updated total amount is displayed.
  [[EarlGrey selectElementWithMatcher:PriceCellMatcher(@"Donation, USD $0.99")]
      assertWithMatcher:grey_notNil()];

  // Reenable EarlGrey's synchronization.
  [[GREYConfiguration sharedInstance]
          setValue:@YES
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
}

// Tests when PaymentRequest.show() is called with a promise, the payment sheet
// is initially displayed with a spinner and the Buy button is not enabled. Once
// the promise rejects, the payment is aborted.
- (void)testBuyWithRejectingPromise {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kShowPromisePage)];

  // Disable EarlGrey's synchronization.
  [[GREYConfiguration sharedInstance]
          setValue:@NO
      forConfigKey:kGREYConfigKeySynchronizationEnabled];

  [ChromeEarlGrey tapWebViewElementWithID:@"buyWithRejectingPromise"];

  // Confirm that the Payment Request UI is showing.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PaymentRequestView()]
      assertWithMatcher:grey_notNil()];

  // Verify that the Buy button is not enabled.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_PAYMENTS_PAY_BUTTON)]
      assertWithMatcher:grey_not(grey_enabled())];

  // Wait until the error screen becomes visible.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(
                                     kPaymentRequestErrorCollectionViewID)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(
      testing::WaitUntilConditionOrTimeout(kShowPromiseTimeout, condition),
      @"Show promise failed to resolve.");

  // Reenable EarlGrey's synchronization.
  [[GREYConfiguration sharedInstance]
          setValue:@YES
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
}

@end
