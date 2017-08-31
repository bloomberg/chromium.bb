// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/histogram_tester.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/payments/core/journey_logger.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/payments/payment_request_egtest_base.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using payments::JourneyLogger;
}  // namespace

// Journey logger tests for Payment Request.
@interface PaymentRequestJourneyLoggerEGTest : PaymentRequestEGTestBase
@end

@implementation PaymentRequestJourneyLoggerEGTest {
  autofill::AutofillProfile _profile;
  autofill::CreditCard _creditCard1;
}

#pragma mark - XCTestCase

// Set up called once before each test.
- (void)setUp {
  [super setUp];

  _profile = autofill::test::GetFullProfile();
  [self addAutofillProfile:_profile];

  _creditCard1 = autofill::test::GetCreditCard();
  _creditCard1.set_billing_address_id(_profile.guid());
  [self addCreditCard:_creditCard1];
}

#pragma mark - Tests

// Tests that the selected instrument metric is correctly logged when the
// Payment Request is completed with a credit card.
- (void)testSelectedPaymentMethod {
  base::HistogramTester histogramTester;

  [self loadTestPage:"payment_request_no_shipping_test.html"];
  [ChromeEarlGrey tapWebViewElementWithID:@"buy"];
  [self payWithCreditCardUsingCVC:@"123"];

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SHOWN, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_COMPLETED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS,
      @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_GOOGLE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_OTHER,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SELECTED_CREDIT_CARD,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_GOOGLE, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER, @"");
}

- (void)testOnlyBobpaySupported {
  base::HistogramTester histogramTester;

  [self loadTestPage:"payment_request_bobpay_test.html"];
  [ChromeEarlGrey tapWebViewElementWithID:@"buy"];
  [ChromeEarlGrey waitForWebViewContainingText:"rejected"];

  histogramTester.ExpectBucketCount(
      "PaymentRequest.CheckoutFunnel.NoShow",
      JourneyLogger::NOT_SHOWN_REASON_NO_SUPPORTED_PAYMENT_METHOD, 1);

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SHOWN, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_COMPLETED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT, @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS,
      @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertFalse(
      buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_GOOGLE,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_OTHER,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_CREDIT_CARD,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_GOOGLE, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER, @"");
}

- (void)testShowSameRequest {
  base::HistogramTester histogramTester;

  [self loadTestPage:"payment_request_multiple_show_test.html"];
  [ChromeEarlGrey tapWebViewElementWithID:@"buy"];
  [ChromeEarlGrey tapWebViewElementWithID:@"showAgain"];
  [self payWithCreditCardUsingCVC:@"123"];

  // Trying to show the same request twice is not considered a concurrent
  // request.
  GREYAssertTrue(
      histogramTester.GetAllSamples("PaymentRequest.CheckoutFunnel.NoShow")
          .empty(),
      @"");

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogramTester.GetAllSamples("PaymentRequest.Events");
  GREYAssertEqual(1U, buckets.size(), @"Exactly one bucket");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SHOWN, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_PAY_CLICKED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_RECEIVED_INSTRUMENT_DETAILS, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW, @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_COMPLETED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_USER_ABORTED, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_OTHER_ABORTED, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_INITIAL_FORM_OF_PAYMENT, @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS,
      @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_SHIPPING, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_NAME,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_PHONE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_PAYER_EMAIL,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_FALSE,
                  @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_CAN_MAKE_PAYMENT_TRUE,
                  @"");
  GREYAssertTrue(
      buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_BASIC_CARD, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_GOOGLE,
                  @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_REQUEST_METHOD_OTHER,
                 @"");
  GREYAssertTrue(buckets[0].min & JourneyLogger::EVENT_SELECTED_CREDIT_CARD,
                 @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_GOOGLE, @"");
  GREYAssertFalse(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER, @"");
}

// TODO(crbug.com/602666): add a test to verify that the correct metrics get
// recorded if the page tries to show() a second PaymentRequest, similar to
// PaymentRequestJourneyLoggerMultipleShowTest.StartNewRequest from
// payment_request_journey_logger_browsertest.cc.

@end
