// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_interactive_uitest_base.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/payment_request.h"

namespace payments {

class PaymentRequestCreditCardEditorTest
    : public PaymentRequestInteractiveTestBase {
 protected:
  PaymentRequestCreditCardEditorTest()
      : PaymentRequestInteractiveTestBase(
            "/payment_request_no_shipping_test.html") {}
 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCreditCardEditorTest);
};

// Flaky on Win.  http://crbug.com/691185
#if defined(OS_WIN)
#define MAYBE_EnteringValidData DISABLED_EnteringValidData
#else
#define MAYBE_EnteringValidData EnteringValidData
#endif
IN_PROC_BROWSER_TEST_F(PaymentRequestCreditCardEditorTest,
                       MAYBE_EnteringValidData) {
  InvokePaymentRequestUI();

  OpenPaymentMethodScreen();

  OpenCreditCardEditorScreen();

  SetEditorTextfieldValue(base::ASCIIToUTF16("Bob Jones"),
                          autofill::CREDIT_CARD_NAME_FULL);
  SetEditorTextfieldValue(base::ASCIIToUTF16("4111111111111111"),
                          autofill::CREDIT_CARD_NUMBER);
  SetEditorTextfieldValue(base::ASCIIToUTF16("05/45"),
                          autofill::CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);

  ResetEventObserver(DialogEvent::BACK_NAVIGATION);

  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager =
      GetPaymentRequests(GetActiveWebContents())[0]->personal_data_manager();
  EXPECT_EQ(1u, personal_data_manager->GetCreditCards().size());
  autofill::CreditCard* credit_card =
      personal_data_manager->GetCreditCards()[0];
  EXPECT_EQ(5, credit_card->expiration_month());
  EXPECT_EQ(2045, credit_card->expiration_year());
  EXPECT_EQ(2045, credit_card->expiration_year());
  EXPECT_EQ(base::ASCIIToUTF16("1111"), credit_card->LastFourDigits());
  EXPECT_EQ(base::ASCIIToUTF16("Bob Jones"),
            credit_card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCreditCardEditorTest,
                       EnteringInvalidData) {
  InvokePaymentRequestUI();

  OpenPaymentMethodScreen();

  OpenCreditCardEditorScreen();

  SetEditorTextfieldValue(base::ASCIIToUTF16("Bob Jones"),
                          autofill::CREDIT_CARD_NAME_FULL);
  SetEditorTextfieldValue(base::ASCIIToUTF16("41111111invalidcard"),
                          autofill::CREDIT_CARD_NUMBER);
  SetEditorTextfieldValue(base::ASCIIToUTF16("05/45"),
                          autofill::CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);

  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);

  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NAME_FULL));
  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));
  EXPECT_FALSE(
      IsEditorTextfieldInvalid(autofill::CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR));

  autofill::PersonalDataManager* personal_data_manager =
      GetPaymentRequests(GetActiveWebContents())[0]->personal_data_manager();
  EXPECT_EQ(0u, personal_data_manager->GetCreditCards().size());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCreditCardEditorTest, EnteringEmptyData) {
  InvokePaymentRequestUI();

  OpenPaymentMethodScreen();

  OpenCreditCardEditorScreen();

  // Setting empty data and unfocusing a required textfield will make it
  // invalid.
  SetEditorTextfieldValue(base::ASCIIToUTF16(""),
                          autofill::CREDIT_CARD_NAME_FULL);

  ValidatingTextfield* textfield =
      static_cast<ValidatingTextfield*>(dialog_view()->GetViewByID(
          static_cast<int>(autofill::CREDIT_CARD_NAME_FULL)));
  EXPECT_TRUE(textfield);
  EXPECT_TRUE(textfield->invalid());
}

}  // namespace payments
