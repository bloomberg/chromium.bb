// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace payments {

class PaymentRequestCanMakePaymentMetricsTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestCanMakePaymentMetricsTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_can_make_payment_metrics_test.html") {}

  void SetupInitialAddressAndCreditCard() {
    autofill::AutofillProfile billing_address =
        autofill::test::GetFullProfile();
    AddAutofillProfile(billing_address);
    autofill::CreditCard card = autofill::test::GetCreditCard();
    card.set_billing_address_id(billing_address.guid());
    AddCreditCard(card);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCanMakePaymentMetricsTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_True_Shown_Completed) {
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address so CanMakePayment
  // returns true.
  SetupInitialAddressAndCreditCard();

  // Start the Payment Request and expect CanMakePayment to be called before the
  // Payment Request is shown.
  ResetEventObserverForSequence(
      {DialogEvent::CAN_MAKE_PAYMENT_CALLED, DialogEvent::DIALOG_OPENED});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryShow();"));
  WaitForObservedEvent();

  // Complete the Payment Request.
  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectBucketCount("PaymentRequest.CanMakePayment.Usage",
                                     JourneyLogger::CAN_MAKE_PAYMENT_USED, 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.EffectOnShow",
      JourneyLogger::CMP_EFFECT_ON_SHOW_DID_SHOW |
          JourneyLogger::CMP_EFFECT_ON_SHOW_COULD_MAKE_PAYMENT,
      1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.TrueWithShowEffectOnCompletion",
      JourneyLogger::COMPLETION_STATUS_COMPLETED, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_True_Shown_OtherAborted) {
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address so CanMakePayment
  // returns true.
  SetupInitialAddressAndCreditCard();

  // Start the Payment Request and expect CanMakePayment to be called before the
  // Payment Request is shown.
  ResetEventObserverForSequence(
      {DialogEvent::CAN_MAKE_PAYMENT_CALLED, DialogEvent::DIALOG_OPENED});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryShow();"));
  WaitForObservedEvent();

  // Simulate that an unexpected error occurs.
  ResetEventObserverForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(
      content::ExecuteScript(GetActiveWebContents(), click_buy_button_js));
  WaitForObservedEvent();

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectBucketCount("PaymentRequest.CanMakePayment.Usage",
                                     JourneyLogger::CAN_MAKE_PAYMENT_USED, 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.EffectOnShow",
      JourneyLogger::CMP_EFFECT_ON_SHOW_DID_SHOW |
          JourneyLogger::CMP_EFFECT_ON_SHOW_COULD_MAKE_PAYMENT,
      1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.TrueWithShowEffectOnCompletion",
      JourneyLogger::COMPLETION_STATUS_OTHER_ABORTED, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_True_Shown_UserAborted) {
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address so CanMakePayment
  // returns true.
  SetupInitialAddressAndCreditCard();

  // Start the Payment Request and expect CanMakePayment to be called before the
  // Payment Request is shown.
  ResetEventObserverForSequence(
      {DialogEvent::CAN_MAKE_PAYMENT_CALLED, DialogEvent::DIALOG_OPENED});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryShow();"));
  WaitForObservedEvent();

  // Simulate that the user cancels the Payment Request.
  ClickOnCancel();

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectBucketCount("PaymentRequest.CanMakePayment.Usage",
                                     JourneyLogger::CAN_MAKE_PAYMENT_USED, 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.EffectOnShow",
      JourneyLogger::CMP_EFFECT_ON_SHOW_DID_SHOW |
          JourneyLogger::CMP_EFFECT_ON_SHOW_COULD_MAKE_PAYMENT,
      1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.TrueWithShowEffectOnCompletion",
      JourneyLogger::COMPLETION_STATUS_USER_ABORTED, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_False_Shown_Completed) {
  base::HistogramTester histogram_tester;

  // An address is needed so that the UI can choose it as a billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);

  // Don't add a card on file, so CanMakePayment returns false.
  // Start the Payment Request and expect CanMakePayment to be called before the
  // Payment Request is shown.
  ResetEventObserverForSequence(
      {DialogEvent::CAN_MAKE_PAYMENT_CALLED, DialogEvent::DIALOG_OPENED});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryShow();"));
  WaitForObservedEvent();

  // Add a test credit card.
  OpenCreditCardEditorScreen();
  SetEditorTextfieldValue(base::UTF8ToUTF16("Bob Simpson"),
                          autofill::CREDIT_CARD_NAME_FULL);
  SetEditorTextfieldValue(base::UTF8ToUTF16("4111111111111111"),
                          autofill::CREDIT_CARD_NUMBER);
  SetComboboxValue(base::UTF8ToUTF16("05"), autofill::CREDIT_CARD_EXP_MONTH);
  SetComboboxValue(base::UTF8ToUTF16("2026"),
                   autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);
  SelectBillingAddress(billing_address.guid());
  ResetEventObserver(DialogEvent::BACK_TO_PAYMENT_SHEET_NAVIGATION);
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);

  // Complete the Payment Request.
  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectBucketCount("PaymentRequest.CanMakePayment.Usage",
                                     JourneyLogger::CAN_MAKE_PAYMENT_USED, 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.EffectOnShow",
      JourneyLogger::CMP_EFFECT_ON_SHOW_DID_SHOW, 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
      JourneyLogger::COMPLETION_STATUS_COMPLETED, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_False_Shown_OtherAborted) {
  base::HistogramTester histogram_tester;

  // Don't add a card on file, so CanMakePayment returns false.
  // Start the Payment Request and expect CanMakePayment to be called before the
  // Payment Request is shown.
  ResetEventObserverForSequence(
      {DialogEvent::CAN_MAKE_PAYMENT_CALLED, DialogEvent::DIALOG_OPENED});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryShow();"));
  WaitForObservedEvent();

  // Simulate that an unexpected error occurs.
  ResetEventObserverForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(
      content::ExecuteScript(GetActiveWebContents(), click_buy_button_js));
  WaitForObservedEvent();

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectBucketCount("PaymentRequest.CanMakePayment.Usage",
                                     JourneyLogger::CAN_MAKE_PAYMENT_USED, 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.EffectOnShow",
      JourneyLogger::CMP_EFFECT_ON_SHOW_DID_SHOW, 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
      JourneyLogger::COMPLETION_STATUS_OTHER_ABORTED, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_False_Shown_UserAborted) {
  base::HistogramTester histogram_tester;

  // Don't add a card on file, so CanMakePayment returns false.
  // Start the Payment Request and expect CanMakePayment to be called before the
  // Payment Request is shown.
  ResetEventObserverForSequence(
      {DialogEvent::CAN_MAKE_PAYMENT_CALLED, DialogEvent::DIALOG_OPENED});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryShow();"));
  WaitForObservedEvent();

  // Simulate that the user cancels the Payment Request.
  ClickOnCancel();

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectBucketCount("PaymentRequest.CanMakePayment.Usage",
                                     JourneyLogger::CAN_MAKE_PAYMENT_USED, 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.EffectOnShow",
      JourneyLogger::CMP_EFFECT_ON_SHOW_DID_SHOW, 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
      JourneyLogger::COMPLETION_STATUS_USER_ABORTED, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_True_NotShown) {
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address so CanMakePayment
  // returns true.
  SetupInitialAddressAndCreditCard();

  // Try to start the Payment Request, but only CanMakePayment should be called.
  ResetEventObserver(DialogEvent::CAN_MAKE_PAYMENT_CALLED);
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryNoShow();"));
  WaitForObservedEvent();

  // Navigate away to trigger the log.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectBucketCount("PaymentRequest.CanMakePayment.Usage",
                                     JourneyLogger::CAN_MAKE_PAYMENT_USED, 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.EffectOnShow",
      JourneyLogger::CMP_EFFECT_ON_SHOW_COULD_MAKE_PAYMENT, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       Called_False_NotShown) {
  base::HistogramTester histogram_tester;

  // Don't add a card on file, so CanMakePayment returns false.
  // Try to start the Payment Request, but only CanMakePayment should be called.
  ResetEventObserver(DialogEvent::CAN_MAKE_PAYMENT_CALLED);
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryNoShow();"));
  WaitForObservedEvent();

  // Navigate away to trigger the log.
  NavigateTo("/payment_request_email_test.html");

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectBucketCount("PaymentRequest.CanMakePayment.Usage",
                                     JourneyLogger::CAN_MAKE_PAYMENT_USED, 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.EffectOnShow",
      JourneyLogger::CMP_EFFECT_ON_SHOW_COULD_NOT_MAKE_PAYMENT_AND_DID_NOT_SHOW,
      1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       NotCalled_Shown_Completed) {
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address to make it simpler
  // to complete the Payment Request.
  SetupInitialAddressAndCreditCard();

  // Start the Payment Request, CanMakePayment should not be called in this
  // test.
  ResetEventObserver(DialogEvent::DIALOG_OPENED);
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "noQueryShow();"));
  WaitForObservedEvent();

  // Complete the Payment Request.
  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectBucketCount("PaymentRequest.CanMakePayment.Usage",
                                     JourneyLogger::CAN_MAKE_PAYMENT_NOT_USED,
                                     1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
      JourneyLogger::COMPLETION_STATUS_COMPLETED, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       NotCalled_Shown_OtherAborted) {
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address to make it simpler
  // to complete the Payment Request.
  SetupInitialAddressAndCreditCard();

  // Start the Payment Request, CanMakePayment should not be called in this
  // test.
  ResetEventObserver(DialogEvent::DIALOG_OPENED);
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "noQueryShow();"));
  WaitForObservedEvent();

  // Simulate that an unexpected error occurs.
  ResetEventObserverForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(
      content::ExecuteScript(GetActiveWebContents(), click_buy_button_js));
  WaitForObservedEvent();

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectBucketCount("PaymentRequest.CanMakePayment.Usage",
                                     JourneyLogger::CAN_MAKE_PAYMENT_NOT_USED,
                                     1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
      JourneyLogger::COMPLETION_STATUS_OTHER_ABORTED, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       NotCalled_Shown_UserAborted) {
  base::HistogramTester histogram_tester;

  // Setup a credit card with an associated billing address to make it simpler
  // to complete the Payment Request.
  SetupInitialAddressAndCreditCard();

  // Start the Payment Request, CanMakePayment should not be called in this
  // test.
  ResetEventObserver(DialogEvent::DIALOG_OPENED);
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "noQueryShow();"));
  WaitForObservedEvent();

  // Simulate that the user cancels the Payment Request.
  ClickOnCancel();

  // Make sure the metrics are logged correctly.
  histogram_tester.ExpectBucketCount("PaymentRequest.CanMakePayment.Usage",
                                     JourneyLogger::CAN_MAKE_PAYMENT_NOT_USED,
                                     1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.NotUsed.WithShowEffectOnCompletion",
      JourneyLogger::COMPLETION_STATUS_USER_ABORTED, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       UserAborted_NavigationToSameOrigin) {
  base::HistogramTester histogram_tester;

  // Start the Payment Request and expect CanMakePayment to be called before the
  // Payment Request is shown.
  ResetEventObserverForSequence(
      {DialogEvent::CAN_MAKE_PAYMENT_CALLED, DialogEvent::DIALOG_OPENED});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryShow();"));
  WaitForObservedEvent();

  // Simulate that the user navigates away from the Payment Request by opening a
  // different page on the same origin.
  ResetEventObserverForSequence({DialogEvent::DIALOG_CLOSED});
  NavigateTo("/payment_request_email_test.html");
  WaitForObservedEvent();

  // Make sure that a navigation is logged as a user abort.
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
      JourneyLogger::COMPLETION_STATUS_USER_ABORTED, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       UserAborted_NavigationToDifferentOrigin) {
  base::HistogramTester histogram_tester;

  // Start the Payment Request and expect CanMakePayment to be called before the
  // Payment Request is shown.
  ResetEventObserverForSequence(
      {DialogEvent::CAN_MAKE_PAYMENT_CALLED, DialogEvent::DIALOG_OPENED});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryShow();"));
  WaitForObservedEvent();

  // Simulate that the user navigates away from the Payment Request by opening a
  // different page on a different origin.
  ResetEventObserverForSequence({DialogEvent::DIALOG_CLOSED});
  GURL other_origin_url =
      https_server()->GetURL("b.com", "/payment_request_email_test.html");
  ui_test_utils::NavigateToURL(browser(), other_origin_url);
  WaitForObservedEvent();

  // Make sure that a navigation is logged as a user abort.
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
      JourneyLogger::COMPLETION_STATUS_USER_ABORTED, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       UserAborted_TabClose) {
  base::HistogramTester histogram_tester;

  // Start the Payment Request and expect CanMakePayment to be called before the
  // Payment Request is shown.
  ResetEventObserverForSequence(
      {DialogEvent::CAN_MAKE_PAYMENT_CALLED, DialogEvent::DIALOG_OPENED});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryShow();"));
  WaitForObservedEvent();

  // Simulate that the user closes the tab containing the Payment Request.
  ResetEventObserverForSequence({DialogEvent::DIALOG_CLOSED});
  chrome::CloseTab(browser());
  WaitForObservedEvent();

  // Make sure that a tab closing is logged as a user abort.
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
      JourneyLogger::COMPLETION_STATUS_USER_ABORTED, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentMetricsTest,
                       UserAborted_Reload) {
  base::HistogramTester histogram_tester;

  // Start the Payment Request and expect CanMakePayment to be called before the
  // Payment Request is shown.
  ResetEventObserverForSequence(
      {DialogEvent::CAN_MAKE_PAYMENT_CALLED, DialogEvent::DIALOG_OPENED});
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "queryShow();"));
  WaitForObservedEvent();

  // Simulate that the user reloads the page containing the Payment Request.
  ResetEventObserverForSequence({DialogEvent::DIALOG_CLOSED});
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  WaitForObservedEvent();

  // Make sure that a reload is logged as a user abort.
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CanMakePayment.Used.FalseWithShowEffectOnCompletion",
      JourneyLogger::COMPLETION_STATUS_USER_ABORTED, 1);
}

}  // namespace payments
