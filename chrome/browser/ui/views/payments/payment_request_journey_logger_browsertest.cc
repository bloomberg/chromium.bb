// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/payments/core/journey_logger.h"

namespace payments {

class PaymentRequestJourneyLoggerSelectedPaymentInstrumentTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestJourneyLoggerSelectedPaymentInstrumentTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_no_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      PaymentRequestJourneyLoggerSelectedPaymentInstrumentTest);
};

// Tests that the SelectedPaymentInstrument metrics is correctly logged when the
// Payment Request is completed with a credit card.
IN_PROC_BROWSER_TEST_F(PaymentRequestJourneyLoggerSelectedPaymentInstrumentTest,
                       TestSelectedPaymentMethod) {
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Complete the Payment Request.
  InvokePaymentRequestUI();
  ResetEventObserver(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  // Expect a credit card as the selected payment instrument in the metrics.
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.SelectedPaymentMethod",
      JourneyLogger::SELECTED_PAYMENT_METHOD_CREDIT_CARD, 1);
}

}  // namespace payments
