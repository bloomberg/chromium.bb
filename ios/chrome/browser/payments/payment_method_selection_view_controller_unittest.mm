// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_method_selection_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PaymentMethodSelectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    personal_data_manager_ =
        base::MakeUnique<autofill::TestPersonalDataManager>();
    // Add testing credit cards. autofill::TestPersonalDataManager does not take
    // ownership of the cards.
    credit_card1_ = payment_request_test_util::CreateTestCreditCard();
    personal_data_manager_->AddTestingCreditCard(credit_card1_.get());
    credit_card2_ = payment_request_test_util::CreateTestCreditCard();
    personal_data_manager_->AddTestingCreditCard(credit_card2_.get());

    web::PaymentRequest web_payment_request =
        payment_request_test_util::CreateTestWebPaymentRequest();

    payment_request_ = base::MakeUnique<PaymentRequest>(
        base::MakeUnique<web::PaymentRequest>(web_payment_request),
        personal_data_manager_.get());

    return [[PaymentMethodSelectionViewController alloc]
        initWithPaymentRequest:payment_request_.get()];
  }

  PaymentMethodSelectionViewController*
  GetPaymentMethodSelectionViewController() {
    return base::mac::ObjCCastStrict<PaymentMethodSelectionViewController>(
        controller());
  }

  std::unique_ptr<autofill::CreditCard> credit_card1_;
  std::unique_ptr<autofill::CreditCard> credit_card2_;
  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_manager_;
  std::unique_ptr<PaymentRequest> payment_request_;
};

// Tests that the correct number of items are displayed after loading the model
// and that the correct item appears to be selected.
TEST_F(PaymentMethodSelectionViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_PAYMENTS_METHOD_OF_PAYMENT_LABEL);

  [GetPaymentMethodSelectionViewController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // One item for each of payment method plus 1 for the add button.
  ASSERT_EQ(3U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The first two items should be of type PaymentMethodItem. The first one
  // should appear to be selected.
  id item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[PaymentMethodItem class]]);
  PaymentMethodItem* payment_method_item = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryCheckmark,
            payment_method_item.accessoryType);

  item = GetCollectionViewItem(0, 1);
  EXPECT_TRUE([item isMemberOfClass:[PaymentMethodItem class]]);
  payment_method_item = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone,
            payment_method_item.accessoryType);

  // The last item should be of type TextAndMessageItem.
  item = GetCollectionViewItem(0, 2);
  EXPECT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
}
