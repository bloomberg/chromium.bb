// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_interactive_uitest_base.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/content/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentMethodViewControllerTest
    : public PaymentRequestInteractiveTestBase {
 protected:
  PaymentMethodViewControllerTest()
      : PaymentRequestInteractiveTestBase(
            "/payment_request_no_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentMethodViewControllerTest);
};

IN_PROC_BROWSER_TEST_F(PaymentMethodViewControllerTest, EmptyList) {
  InvokePaymentRequestUI();
  OpenPaymentMethodScreen();

  views::View* list_view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_SHEET_LIST_VIEW));
  EXPECT_TRUE(list_view);
  EXPECT_FALSE(list_view->has_children());
}

IN_PROC_BROWSER_TEST_F(PaymentMethodViewControllerTest, OneCardSelected) {
  const autofill::CreditCard card = autofill::test::GetCreditCard();
  AddCreditCard(card);

  InvokePaymentRequestUI();
  OpenPaymentMethodScreen();

  PaymentRequest* request = GetPaymentRequests(GetActiveWebContents())[0];
  EXPECT_EQ(1U, request->credit_cards().size());

  views::View* list_view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_SHEET_LIST_VIEW));
  EXPECT_TRUE(list_view);
  EXPECT_EQ(1, list_view->child_count());

  EXPECT_EQ(card, *request->selected_credit_card());
  views::View* checkmark_view = list_view->child_at(0)->GetViewByID(
      static_cast<int>(DialogViewID::CHECKMARK_VIEW));
  EXPECT_TRUE(checkmark_view->visible());
}

IN_PROC_BROWSER_TEST_F(PaymentMethodViewControllerTest,
                       OneCardSelectedOutOfMany) {
  autofill::CreditCard card1 = autofill::test::GetCreditCard();
  // Ensure that this card is the first suggestion.
  card1.set_use_count(5U);
  AddCreditCard(card1);

  autofill::CreditCard card2 = autofill::test::GetCreditCard2();
  card2.set_use_count(1U);
  AddCreditCard(card2);

  InvokePaymentRequestUI();
  OpenPaymentMethodScreen();

  PaymentRequest* request = GetPaymentRequests(GetActiveWebContents())[0];
  EXPECT_EQ(2U, request->credit_cards().size());
  EXPECT_EQ(card1, *request->selected_credit_card());

  views::View* list_view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_SHEET_LIST_VIEW));
  EXPECT_TRUE(list_view);
  EXPECT_EQ(2, list_view->child_count());

  EXPECT_EQ(card1, *request->selected_credit_card());
  views::View* checkmark_view = list_view->child_at(0)->GetViewByID(
      static_cast<int>(DialogViewID::CHECKMARK_VIEW));
  EXPECT_TRUE(checkmark_view->visible());

  views::View* checkmark_view2 = list_view->child_at(1)->GetViewByID(
      static_cast<int>(DialogViewID::CHECKMARK_VIEW));
  EXPECT_FALSE(checkmark_view2->visible());

  // Simulate selecting the second card.
  ClickOnDialogViewAndWait(list_view->child_at(1));

  EXPECT_EQ(card2, *request->selected_credit_card());
  EXPECT_FALSE(checkmark_view->visible());
  EXPECT_TRUE(checkmark_view2->visible());

  // Clicking on the second card again should not modify any state.
  ClickOnDialogViewAndWait(list_view->child_at(1));

  EXPECT_EQ(card2, *request->selected_credit_card());
  EXPECT_FALSE(checkmark_view->visible());
  EXPECT_TRUE(checkmark_view2->visible());
}

}  // namespace payments
