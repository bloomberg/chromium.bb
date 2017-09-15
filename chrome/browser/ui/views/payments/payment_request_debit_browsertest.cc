// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

constexpr auto CREDIT = ::autofill::CreditCard::CardType::CARD_TYPE_CREDIT;
constexpr auto DEBIT = ::autofill::CreditCard::CardType::CARD_TYPE_DEBIT;
constexpr auto PREPAID = ::autofill::CreditCard::CardType::CARD_TYPE_PREPAID;
constexpr auto UNKNOWN = ::autofill::CreditCard::CardType::CARD_TYPE_UNKNOWN;

// Tests for a merchant that requests a debit card.
class PaymentRequestDebitTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestDebitTest()
      : PaymentRequestBrowserTestBase("/payment_request_debit_test.html") {}

  const std::string& GetOrCreateBillingAddressId() {
    if (billing_address_id_.empty()) {
      autofill::AutofillProfile billing_address =
          autofill::test::GetFullProfile();
      billing_address_id_ = billing_address.guid();
      AddAutofillProfile(billing_address);
    }
    return billing_address_id_;
  }

  void AddServerCardWithType(autofill::CreditCard::CardType card_type) {
    autofill::CreditCard card = autofill::test::GetMaskedServerCard();
    card.set_card_type(card_type);
    card.set_billing_address_id(GetOrCreateBillingAddressId());
    AddCreditCard(card);
  }

  void CallCanMakePayment() {
    ResetEventObserverForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                   DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(
        content::ExecuteScript(GetActiveWebContents(), "canMakePayment();"));
    WaitForObservedEvent();
  }

 private:
  std::string billing_address_id_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestDebitTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestDebitTest, CanMakePaymentWithDebitCard) {
  AddServerCardWithType(DEBIT);
  CallCanMakePayment();
  ExpectBodyContains({"true"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestDebitTest,
                       CanMakePaymentWithUnknownCardType) {
  AddServerCardWithType(UNKNOWN);
  CallCanMakePayment();
  ExpectBodyContains({"true"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestDebitTest,
                       CannotMakePaymentWithCreditAndPrepaidCard) {
  AddServerCardWithType(CREDIT);
  AddServerCardWithType(PREPAID);
  CallCanMakePayment();
  ExpectBodyContains({"false"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestDebitTest, DebitCardIsPreselected) {
  AddServerCardWithType(DEBIT);
  CallCanMakePayment();
  InvokePaymentRequestUI();
  EXPECT_TRUE(IsPayButtonEnabled());
  ClickOnCancel();
}

IN_PROC_BROWSER_TEST_F(PaymentRequestDebitTest,
                       UnknownCardTypeIsNotPreselected) {
  AddServerCardWithType(UNKNOWN);
  InvokePaymentRequestUI();
  EXPECT_FALSE(IsPayButtonEnabled());
  ClickOnCancel();
}

IN_PROC_BROWSER_TEST_F(PaymentRequestDebitTest, PayWithLocalCard) {
  // All local cards have "unknown" card type by design.
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(GetOrCreateBillingAddressId());
  AddCreditCard(card);
  InvokePaymentRequestUI();

  // The local card of "unknown" type is not pre-selected.
  EXPECT_FALSE(IsPayButtonEnabled());

  // Select the local card and click the "Pay" button.
  OpenPaymentMethodScreen();
  ResetEventObserver(DialogEvent::BACK_NAVIGATION);
  ClickOnChildInListViewAndWait(/*child_index=*/0, /*num_children=*/1,
                                DialogViewID::PAYMENT_METHOD_SHEET_LIST_VIEW);
  EXPECT_TRUE(IsPayButtonEnabled());

  // Type in the CVC number and verify that it's sent to the page.
  ResetEventObserver(DialogEvent::DIALOG_CLOSED);
  PayWithCreditCardAndWait(base::ASCIIToUTF16("012"));
  ExpectBodyContains({"\"cardSecurityCode\": \"012\""});
}

}  // namespace payments
