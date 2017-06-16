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
#include "content/public/test/browser_test_utils.h"

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

  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Initiated",
                                      1, 1);
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Shown", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CheckoutFunnel.PayClicked", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CheckoutFunnel.ReceivedInstrumentDetails", 1, 1);
  // Expect a credit card as the selected payment instrument in the metrics.
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.SelectedPaymentMethod",
      JourneyLogger::SELECTED_PAYMENT_METHOD_CREDIT_CARD, 1);
}

class PaymentRequestJourneyLoggerNoSupportedPaymentMethodTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestJourneyLoggerNoSupportedPaymentMethodTest()
      : PaymentRequestBrowserTestBase("/payment_request_bobpay_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      PaymentRequestJourneyLoggerNoSupportedPaymentMethodTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestJourneyLoggerNoSupportedPaymentMethodTest,
                       OnlyBobpaySupported) {
  base::HistogramTester histogram_tester;

  ResetEventObserver(DialogEvent::NOT_SUPPORTED_ERROR);
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { document.getElementById('buy').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));
  WaitForObservedEvent();

  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Initiated",
                                      1, 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CheckoutFunnel.NoShow",
      JourneyLogger::NOT_SHOWN_REASON_NO_SUPPORTED_PAYMENT_METHOD, 1);
}

class PaymentRequestJourneyLoggerMultipleShowTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestJourneyLoggerMultipleShowTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_multiple_show_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestJourneyLoggerMultipleShowTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestJourneyLoggerMultipleShowTest,
                       ShowSameRequest) {
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Start a Payment Request.
  InvokePaymentRequestUI();

  // Try to show it again.
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { document.getElementById('showAgain').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));

  // Complete the original Payment Request.
  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  // Trying to show the same request twice is not considered a concurrent
  // request.
  EXPECT_TRUE(
      histogram_tester.GetAllSamples("PaymentRequest.CheckoutFunnel.NoShow")
          .empty());

  // Expect that other metrics were logged correctly.
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Initiated",
                                      1, 1);
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Shown", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CheckoutFunnel.PayClicked", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CheckoutFunnel.ReceivedInstrumentDetails", 1, 1);
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Completed",
                                      1, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestJourneyLoggerMultipleShowTest,
                       StartNewRequest) {
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Start a Payment Request.
  InvokePaymentRequestUI();

  // Get the dialog view of the first request, since the one cached for the
  // tests will be replaced by the second Payment Request.
  PaymentRequestDialogView* first_dialog_view = dialog_view();

  // Try to show a second request.
  ResetEventObserver(DialogEvent::DIALOG_CLOSED);
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { document.getElementById('showSecondRequest').click(); "
      "})();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));
  WaitForObservedEvent();

  // Complete the original Payment Request.
  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"), first_dialog_view);

  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Initiated",
                                      1, 2);
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Shown", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CheckoutFunnel.PayClicked", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CheckoutFunnel.ReceivedInstrumentDetails", 1, 1);

  // The metrics should show that the original Payment Request should be
  // completed and the second one should not have been shown.
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Completed",
                                      1, 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CheckoutFunnel.NoShow",
      JourneyLogger::NOT_SHOWN_REASON_CONCURRENT_REQUESTS, 1);
}

}  // namespace payments
