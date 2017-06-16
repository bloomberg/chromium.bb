// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace payments {

class PaymentRequestCompletionStatusMetricsTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestCompletionStatusMetricsTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_can_make_payment_metrics_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCompletionStatusMetricsTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest, Completed) {
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address so CanMakePayment
  // returns true.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  // Start the Payment Request and expect CanMakePayment to be called before the
  // Payment Request is shown.
  ResetEventObserverForSequence(
      {DialogEvent::CAN_MAKE_PAYMENT_CALLED, DialogEvent::DIALOG_OPENED});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryShow();"));
  WaitForObservedEvent();

  // Complete the Payment Request.
  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  // Make sure the metrics are logged correctly.
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

IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest,
                       MerchantAborted_Reload) {
  base::HistogramTester histogram_tester;

  // Start the Payment Request.
  ResetEventObserver(DialogEvent::DIALOG_OPENED);
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "noQueryShow();"));
  WaitForObservedEvent();

  // The merchant reloads the page.
  ResetEventObserver(DialogEvent::DIALOG_CLOSED);
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                     "(function() { location.reload(); })();"));
  WaitForObservedEvent();

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Initiated",
                                      1, 1);
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Shown", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CheckoutFunnel.Aborted",
      JourneyLogger::ABORT_REASON_MERCHANT_NAVIGATION, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest,
                       MerchantAborted_Navigation) {
  base::HistogramTester histogram_tester;

  // Start the Payment Request.
  ResetEventObserver(DialogEvent::DIALOG_OPENED);
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "noQueryShow();"));
  WaitForObservedEvent();

  // The merchant navigates away.
  ResetEventObserver(DialogEvent::DIALOG_CLOSED);
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                     "(function() { window.location.href = "
                                     "'/payment_request_email_test.html'; "
                                     "})();"));
  WaitForObservedEvent();

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Initiated",
                                      1, 1);
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Shown", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CheckoutFunnel.Aborted",
      JourneyLogger::ABORT_REASON_MERCHANT_NAVIGATION, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest,
                       MerchantAborted_Abort) {
  base::HistogramTester histogram_tester;

  // Start the Payment Request.
  ResetEventObserver(DialogEvent::DIALOG_OPENED);
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "noQueryShow();"));
  WaitForObservedEvent();

  // The merchant aborts the Payment Request.
  ResetEventObserverForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(
      content::ExecuteScript(GetActiveWebContents(), click_buy_button_js));
  WaitForObservedEvent();

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Initiated",
                                      1, 1);
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Shown", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CheckoutFunnel.Aborted",
      JourneyLogger::ABORT_REASON_ABORTED_BY_MERCHANT, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest,
                       UserAborted_Navigation) {
  base::HistogramTester histogram_tester;

  // Start the Payment Request.
  ResetEventObserver(DialogEvent::DIALOG_OPENED);
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "noQueryShow();"));
  WaitForObservedEvent();

  // Navigate away.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Initiated",
                                      1, 1);
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Shown", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CheckoutFunnel.Aborted",
      JourneyLogger::ABORT_REASON_USER_NAVIGATION, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest,
                       UserAborted_CancelButton) {
  base::HistogramTester histogram_tester;

  // Start the Payment Request.
  ResetEventObserver(DialogEvent::DIALOG_OPENED);
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "noQueryShow();"));
  WaitForObservedEvent();

  // Click on the cancel button.
  ClickOnCancel();

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Initiated",
                                      1, 1);
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Shown", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CheckoutFunnel.Aborted",
      JourneyLogger::ABORT_REASON_ABORTED_BY_USER, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest,
                       UserAborted_TabClosed) {
  base::HistogramTester histogram_tester;

  // Start the Payment Request.
  ResetEventObserver(DialogEvent::DIALOG_OPENED);
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "noQueryShow();"));
  WaitForObservedEvent();

  // Close the tab containing the Payment Request.
  ResetEventObserverForSequence({DialogEvent::DIALOG_CLOSED});
  chrome::CloseTab(browser());
  WaitForObservedEvent();

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Initiated",
                                      1, 1);
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Shown", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CheckoutFunnel.Aborted",
      JourneyLogger::ABORT_REASON_ABORTED_BY_USER, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCompletionStatusMetricsTest,
                       UserAborted_Reload) {
  base::HistogramTester histogram_tester;

  // Start the Payment Request.
  ResetEventObserver(DialogEvent::DIALOG_OPENED);
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "noQueryShow();"));
  WaitForObservedEvent();

  // Reload the page containing the Payment Request.
  ResetEventObserverForSequence({DialogEvent::DIALOG_CLOSED});
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  WaitForObservedEvent();

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Initiated",
                                      1, 1);
  histogram_tester.ExpectUniqueSample("PaymentRequest.CheckoutFunnel.Shown", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CheckoutFunnel.Aborted",
      JourneyLogger::ABORT_REASON_USER_NAVIGATION, 1);
}

}  // namespace payments
