// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_interactive_uitest_base.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// A simple PaymentRequest which simply requests 'visa' or 'mastercard' and
// nothing else.
class PaymentSheetViewControllerNoShippingTest
    : public PaymentRequestInteractiveTestBase {
 protected:
  PaymentSheetViewControllerNoShippingTest()
      : PaymentRequestInteractiveTestBase(
            "/payment_request_no_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentSheetViewControllerNoShippingTest);
};

// With no data present, the pay button should be disabled.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerNoShippingTest, NoData) {
  InvokePaymentRequestUI();

  EXPECT_FALSE(IsPayButtonEnabled());
}

// With a supported card (Visa) present, the pay button should be enabled.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerNoShippingTest,
                       SupportedCard) {
  AddCreditCard(autofill::test::GetCreditCard());  // Visa card.

  InvokePaymentRequestUI();
  EXPECT_TRUE(IsPayButtonEnabled());
}

// With only an unsupported card (Amex) in the database, the pay button should
// be disabled.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerNoShippingTest,
                       UnsupportedCard) {
  AddCreditCard(autofill::test::GetCreditCard2());  // Amex card.

  InvokePaymentRequestUI();
  EXPECT_FALSE(IsPayButtonEnabled());
}

// If shipping and contact info are not requested, their rows should not be
// present.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerNoShippingTest,
                       NoShippingNoContactRows) {
  InvokePaymentRequestUI();

  EXPECT_NE(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SUMMARY_SECTION)));
  EXPECT_NE(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION)));
  EXPECT_EQ(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SHIPPING_SECTION)));
  EXPECT_EQ(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION)));
}

// Accepts 'visa' cards and requests the full contact details.
class PaymentSheetViewControllerContactDetailsTest
    : public PaymentRequestInteractiveTestBase {
 protected:
  PaymentSheetViewControllerContactDetailsTest()
      : PaymentRequestInteractiveTestBase(
            "/payment_request_contact_details_and_free_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentSheetViewControllerContactDetailsTest);
};

// With no data present, the pay button should be disabled.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest, NoData) {
  InvokePaymentRequestUI();

  EXPECT_FALSE(IsPayButtonEnabled());
}

// With a supported card (Visa) present, the pay button is still disabled
// because there is no contact details.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       SupportedCard_NoContactInfo) {
  AddCreditCard(autofill::test::GetCreditCard());  // Visa card.

  InvokePaymentRequestUI();
  EXPECT_FALSE(IsPayButtonEnabled());
}

// With a supported card (Visa) present and a complete address profile, there is
// enough information to enable the pay button.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       SupportedCard_CompleteContactInfo) {
  AddCreditCard(autofill::test::GetCreditCard());  // Visa card.
  AddAutofillProfile(autofill::test::GetFullProfile());

  InvokePaymentRequestUI();
  EXPECT_TRUE(IsPayButtonEnabled());
}

// With only an unsupported card present and a complete address profile, the pay
// button is disabled.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       UnsupportedCard_CompleteContactInfo) {
  AddCreditCard(autofill::test::GetCreditCard2());  // Amex card.
  AddAutofillProfile(autofill::test::GetFullProfile());

  InvokePaymentRequestUI();
  EXPECT_FALSE(IsPayButtonEnabled());
}

// With a supported card (Visa) present and a *incomplete* address profile, the
// pay button is disabled.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       SupportedCard_IncompleteContactInfo) {
  AddCreditCard(autofill::test::GetCreditCard());  // Visa card.

  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  // Remove the name from the profile to be stored.
  profile.SetRawInfo(autofill::NAME_FIRST, base::ASCIIToUTF16(""));
  profile.SetRawInfo(autofill::NAME_MIDDLE, base::ASCIIToUTF16(""));
  profile.SetRawInfo(autofill::NAME_LAST, base::ASCIIToUTF16(""));
  AddAutofillProfile(profile);

  InvokePaymentRequestUI();
  EXPECT_FALSE(IsPayButtonEnabled());
}

// If shipping and contact info are requested, show all the rows.
IN_PROC_BROWSER_TEST_F(PaymentSheetViewControllerContactDetailsTest,
                       AllRowsPresent) {
  InvokePaymentRequestUI();

  EXPECT_NE(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SUMMARY_SECTION)));
  EXPECT_NE(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION)));
  EXPECT_NE(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_SHIPPING_SECTION)));
  EXPECT_NE(nullptr, dialog_view()->GetViewByID(static_cast<int>(
                         DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION)));
}

}  // namespace payments
