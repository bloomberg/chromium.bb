// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

namespace {

const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);

}  // namespace

class PaymentRequestCreditCardEditorTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestCreditCardEditorTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_no_shipping_test.html") {}

  PersonalDataLoadedObserverMock personal_data_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCreditCardEditorTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestCreditCardEditorTest, EnteringValidData) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  InvokePaymentRequestUI();

  OpenPaymentMethodScreen();

  OpenCreditCardEditorScreen();

  SetEditorTextfieldValue(base::ASCIIToUTF16("Bob Jones"),
                          autofill::CREDIT_CARD_NAME_FULL);
  SetEditorTextfieldValue(base::ASCIIToUTF16("4111111111111111"),
                          autofill::CREDIT_CARD_NUMBER);
  SetComboboxValue(base::ASCIIToUTF16("05"), autofill::CREDIT_CARD_EXP_MONTH);
  SetComboboxValue(base::ASCIIToUTF16("2026"),
                   autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  ResetEventObserver(DialogEvent::BACK_NAVIGATION);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  data_loop.Run();

  EXPECT_EQ(1u, personal_data_manager->GetCreditCards().size());
  autofill::CreditCard* credit_card =
      personal_data_manager->GetCreditCards()[0];
  EXPECT_EQ(5, credit_card->expiration_month());
  EXPECT_EQ(2026, credit_card->expiration_year());
  EXPECT_EQ(base::ASCIIToUTF16("1111"), credit_card->LastFourDigits());
  EXPECT_EQ(base::ASCIIToUTF16("Bob Jones"),
            credit_card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCreditCardEditorTest,
                       EnteringExpiredCard) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  InvokePaymentRequestUI();

  OpenPaymentMethodScreen();

  OpenCreditCardEditorScreen();

  SetEditorTextfieldValue(base::ASCIIToUTF16("Bob Jones"),
                          autofill::CREDIT_CARD_NAME_FULL);
  SetEditorTextfieldValue(base::ASCIIToUTF16("4111111111111111"),
                          autofill::CREDIT_CARD_NUMBER);
  // The card is expired.
  SetComboboxValue(base::ASCIIToUTF16("01"), autofill::CREDIT_CARD_EXP_MONTH);
  SetComboboxValue(base::ASCIIToUTF16("2017"),
                   autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);

  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NAME_FULL));
  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));
  // TODO(mathp): Both expiration fields should be marked as invalid when the
  // card is expired.
  EXPECT_FALSE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_MONTH));
  EXPECT_FALSE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR));

  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  EXPECT_EQ(0u, personal_data_manager->GetCreditCards().size());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCreditCardEditorTest,
                       EnteringNothingInARequiredField) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  InvokePaymentRequestUI();

  OpenPaymentMethodScreen();

  OpenCreditCardEditorScreen();

  // This field is required. Entering nothing and blurring out will show
  // "Required field".
  SetEditorTextfieldValue(base::ASCIIToUTF16(""), autofill::CREDIT_CARD_NUMBER);
  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PAYMENTS_FIELD_REQUIRED_VALIDATION_MESSAGE),
      GetErrorLabelForType(autofill::CREDIT_CARD_NUMBER));

  // Set the value to something which is not a valid card number. The "invalid
  // card number" string takes precedence over "required field"
  SetEditorTextfieldValue(base::ASCIIToUTF16("41111111invalidcard"),
                          autofill::CREDIT_CARD_NUMBER);
  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PAYMENTS_CARD_NUMBER_INVALID_VALIDATION_MESSAGE),
            GetErrorLabelForType(autofill::CREDIT_CARD_NUMBER));

  // Set the value to a valid number now. No more errors!
  SetEditorTextfieldValue(base::ASCIIToUTF16("4111111111111111"),
                          autofill::CREDIT_CARD_NUMBER);
  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));
  EXPECT_EQ(base::ASCIIToUTF16(""),
            GetErrorLabelForType(autofill::CREDIT_CARD_NUMBER));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCreditCardEditorTest,
                       EnteringInvalidCardNumber) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  InvokePaymentRequestUI();

  OpenPaymentMethodScreen();

  OpenCreditCardEditorScreen();

  SetEditorTextfieldValue(base::ASCIIToUTF16("Bob Jones"),
                          autofill::CREDIT_CARD_NAME_FULL);
  SetEditorTextfieldValue(base::ASCIIToUTF16("41111111invalidcard"),
                          autofill::CREDIT_CARD_NUMBER);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PAYMENTS_CARD_NUMBER_INVALID_VALIDATION_MESSAGE),
            GetErrorLabelForType(autofill::CREDIT_CARD_NUMBER));
  SetComboboxValue(base::ASCIIToUTF16("05"), autofill::CREDIT_CARD_EXP_MONTH);
  SetComboboxValue(base::ASCIIToUTF16("2026"),
                   autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);

  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NAME_FULL));
  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));
  EXPECT_FALSE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_MONTH));
  EXPECT_FALSE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR));

  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  EXPECT_EQ(0u, personal_data_manager->GetCreditCards().size());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCreditCardEditorTest,
                       EnteringUnsupportedCardType) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  InvokePaymentRequestUI();

  OpenPaymentMethodScreen();

  OpenCreditCardEditorScreen();

  SetEditorTextfieldValue(base::ASCIIToUTF16("Bob Jones"),
                          autofill::CREDIT_CARD_NAME_FULL);
  // In this test case, only "visa" and "mastercard" are supported, so entering
  // a MIR card will fail.
  SetEditorTextfieldValue(base::ASCIIToUTF16("22222222invalidcard"),
                          autofill::CREDIT_CARD_NUMBER);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PAYMENTS_VALIDATION_UNSUPPORTED_CREDIT_CARD_TYPE),
            GetErrorLabelForType(autofill::CREDIT_CARD_NUMBER));
  SetComboboxValue(base::ASCIIToUTF16("05"), autofill::CREDIT_CARD_EXP_MONTH);
  SetComboboxValue(base::ASCIIToUTF16("2026"),
                   autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);

  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NAME_FULL));
  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));
  EXPECT_FALSE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_MONTH));
  EXPECT_FALSE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR));

  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  EXPECT_EQ(0u, personal_data_manager->GetCreditCards().size());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestCreditCardEditorTest,
                       EnteringInvalidCardNumber_AndFixingIt) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  InvokePaymentRequestUI();

  OpenPaymentMethodScreen();

  OpenCreditCardEditorScreen();

  SetEditorTextfieldValue(base::ASCIIToUTF16("Bob Jones"),
                          autofill::CREDIT_CARD_NAME_FULL);
  SetEditorTextfieldValue(base::ASCIIToUTF16("41111111invalidcard"),
                          autofill::CREDIT_CARD_NUMBER);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PAYMENTS_CARD_NUMBER_INVALID_VALIDATION_MESSAGE),
            GetErrorLabelForType(autofill::CREDIT_CARD_NUMBER));
  SetComboboxValue(base::ASCIIToUTF16("05"), autofill::CREDIT_CARD_EXP_MONTH);
  SetComboboxValue(base::ASCIIToUTF16("2026"),
                   autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);

  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);

  EXPECT_FALSE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NAME_FULL));
  EXPECT_TRUE(IsEditorTextfieldInvalid(autofill::CREDIT_CARD_NUMBER));
  EXPECT_FALSE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_MONTH));
  EXPECT_FALSE(IsEditorComboboxInvalid(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Fixing the card number.
  SetEditorTextfieldValue(base::ASCIIToUTF16("4111111111111111"),
                          autofill::CREDIT_CARD_NUMBER);
  // The error message has gone.
  EXPECT_EQ(base::ASCIIToUTF16(""),
            GetErrorLabelForType(autofill::CREDIT_CARD_NUMBER));

  // Verifying the data is in the DB.
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();
  personal_data_manager->AddObserver(&personal_data_observer_);

  ResetEventObserver(DialogEvent::BACK_NAVIGATION);

  // Wait until the web database has been updated and the notification sent.
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);
  data_loop.Run();

  EXPECT_EQ(1u, personal_data_manager->GetCreditCards().size());
  autofill::CreditCard* credit_card =
      personal_data_manager->GetCreditCards()[0];
  EXPECT_EQ(5, credit_card->expiration_month());
  EXPECT_EQ(2026, credit_card->expiration_year());
  EXPECT_EQ(base::ASCIIToUTF16("1111"), credit_card->LastFourDigits());
  EXPECT_EQ(base::ASCIIToUTF16("Bob Jones"),
            credit_card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));
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
