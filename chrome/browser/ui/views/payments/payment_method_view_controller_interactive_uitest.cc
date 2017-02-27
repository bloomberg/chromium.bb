// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_interactive_uitest_base.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/payment_request.h"
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
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();

  PersonalDataLoadedObserverMock personal_data_observer;
  personal_data_manager->AddObserver(&personal_data_observer);
  base::RunLoop data_loop;
  EXPECT_CALL(personal_data_observer, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&data_loop));
  autofill::CreditCard card = autofill::test::GetCreditCard();
  personal_data_manager->AddCreditCard(card);
  data_loop.Run();

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
      static_cast<int>(DialogViewID::PAYMENT_METHOD_ITEM_CHECKMARK_VIEW));
  EXPECT_TRUE(checkmark_view->visible());
}

IN_PROC_BROWSER_TEST_F(PaymentMethodViewControllerTest,
                       OneCardSelectedOutOfMany) {
  autofill::PersonalDataManager* personal_data_manager = GetDataManager();

  PersonalDataLoadedObserverMock personal_data_observer1;
  personal_data_manager->AddObserver(&personal_data_observer1);
  base::RunLoop card1_loop;
  EXPECT_CALL(personal_data_observer1, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&card1_loop));
  autofill::CreditCard card1 = autofill::test::GetCreditCard();
  // Ensure that this card is the first suggestion.
  card1.set_use_count(5U);
  personal_data_manager->AddCreditCard(card1);
  card1_loop.Run();
  personal_data_manager->RemoveObserver(&personal_data_observer1);

  EXPECT_EQ(1U, personal_data_manager->GetCreditCardsToSuggest().size());

  PersonalDataLoadedObserverMock personal_data_observer2;
  personal_data_manager->AddObserver(&personal_data_observer2);
  base::RunLoop card2_loop;
  EXPECT_CALL(personal_data_observer2, OnPersonalDataChanged())
      .WillOnce(QuitMessageLoop(&card2_loop));
  autofill::CreditCard card2 = autofill::test::GetCreditCard2();
  card2.set_use_count(1U);
  personal_data_manager->AddCreditCard(card2);
  card2_loop.Run();
  personal_data_manager->RemoveObserver(&personal_data_observer2);

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
      static_cast<int>(DialogViewID::PAYMENT_METHOD_ITEM_CHECKMARK_VIEW));
  EXPECT_TRUE(checkmark_view->visible());

  views::View* checkmark_view2 = list_view->child_at(1)->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_ITEM_CHECKMARK_VIEW));
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
