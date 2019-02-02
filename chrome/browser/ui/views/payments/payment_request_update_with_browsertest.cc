// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestUpdateWithTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestUpdateWithTest() {}

  void RunJavaScriptFunctionToOpenPaymentRequestUI(
      const std::string& function_name) {
    ResetEventWaiterForDialogOpened();

    content::WebContents* web_contents = GetActiveWebContents();
    ASSERT_TRUE(content::ExecuteScript(web_contents, function_name + "();"));

    WaitForObservedEvent();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestUpdateWithTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithTest,
                       UpdateWithNoShippingOptions) {
  NavigateTo("/payment_request_update_with_test.html");
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  RunJavaScriptFunctionToOpenPaymentRequestUI("updateWithNoShippingOptions");

  OpenOrderSummaryScreen();
  EXPECT_EQ(base::ASCIIToUTF16("$5.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  ClickOnBackArrow();

  OpenShippingAddressSectionScreen();
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING,
                               DialogEvent::BACK_NAVIGATION});
  ClickOnChildInListViewAndWait(
      /* child_index=*/1, /*total_num_children=*/2,
      DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW,
      /*wait_for_animation=*/false);
  // Wait for the animation here explicitly, otherwise
  // ClickOnChildInListViewAndWait tries to install an AnimationDelegate before
  // the animation is kicked off (since that's triggered off of the spec being
  // updated) and this hits a DCHECK.
  WaitForAnimation();

  OpenOrderSummaryScreen();
  EXPECT_EQ(base::ASCIIToUTF16("$10.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  ClickOnBackArrow();

  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  ExpectBodyContains({"freeShipping"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestUpdateWithTest,
                       UpdateWithShippingOptions) {
  NavigateTo("/payment_request_update_with_test.html");
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  AddAutofillProfile(autofill::test::GetFullProfile2());
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  RunJavaScriptFunctionToOpenPaymentRequestUI("updateWithShippingOptions");

  OpenOrderSummaryScreen();
  EXPECT_EQ(base::ASCIIToUTF16("$5.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  ClickOnBackArrow();

  OpenShippingAddressSectionScreen();
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::SPEC_DONE_UPDATING,
                               DialogEvent::BACK_NAVIGATION});
  ClickOnChildInListViewAndWait(
      /* child_index=*/1, /*total_num_children=*/2,
      DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW,
      /*wait_for_animation=*/false);
  // Wait for the animation here explicitly, otherwise
  // ClickOnChildInListViewAndWait tries to install an AnimationDelegate before
  // the animation is kicked off (since that's triggered off of the spec being
  // updated) and this hits a DCHECK.
  WaitForAnimation();

  OpenOrderSummaryScreen();
  EXPECT_EQ(base::ASCIIToUTF16("$10.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  ClickOnBackArrow();

  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  ExpectBodyContains({"updatedShipping"});
}

}  // namespace payments
