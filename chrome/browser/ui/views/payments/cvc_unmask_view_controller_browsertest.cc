// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"

namespace payments {

class PaymentRequestCvcUnmaskViewControllerTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestCvcUnmaskViewControllerTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_no_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCvcUnmaskViewControllerTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestCvcUnmaskViewControllerTest,
                       CvcSentToResponse) {
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa card.
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();
  ResetEventObserver(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(base::ASCIIToUTF16("012"));

  ExpectBodyContains({"\"cardSecurityCode\": \"012\""});
}

// Test that going in the CVC editor, backing out and opening it again to pay
// does not crash.
IN_PROC_BROWSER_TEST_F(PaymentRequestCvcUnmaskViewControllerTest,
                       OpenGoBackOpenPay) {
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa card.
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();
  OpenCVCPromptWithCVC(base::ASCIIToUTF16("012"));

  // Go back before confirming the CVC.
  ClickOnBackArrow();

  // Now pay for real.
  PayWithCreditCardAndWait(base::ASCIIToUTF16("012"));
  ExpectBodyContains({"\"cardSecurityCode\": \"012\""});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCvcUnmaskViewControllerTest,
                       EnterAcceleratorConfirmsCvc) {
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa card.
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();

  ResetEventObserver(DialogEvent::DIALOG_CLOSED);
  // This prevents a timeout in error cases where PAY_BUTTON is disabled.
  ASSERT_TRUE(dialog_view()
                  ->GetViewByID(static_cast<int>(DialogViewID::PAY_BUTTON))
                  ->enabled());
  OpenCVCPromptWithCVC(base::ASCIIToUTF16("012"));

  ResetEventObserver(DialogEvent::DIALOG_CLOSED);
  views::View* cvc_sheet = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::CVC_UNMASK_SHEET));
  cvc_sheet->AcceleratorPressed(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
  WaitForAnimation();
  WaitForObservedEvent();

  ExpectBodyContains({"\"cardSecurityCode\": \"012\""});
}

}  // namespace payments
