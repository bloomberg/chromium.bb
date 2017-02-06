// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_method_selection_view_controller.h"

#include <utility>
#include <vector>

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#import "ios/chrome/browser/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PaymentMethodSelectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    personal_data_manager_ =
        base::MakeUnique<autofill::TestPersonalDataManager>();
    // AddTestingProfile will not take ownership of the cards passed to it.
    credit_card1_ = base::MakeUnique<autofill::CreditCard>();
    autofill::test::SetCreditCardInfo(credit_card1_.get(), "John Doe",
                                      "423456789012" /* Visa */, "01", "2999");
    personal_data_manager_->AddTestingCreditCard(credit_card1_.get());
    credit_card2_ = base::MakeUnique<autofill::CreditCard>();
    autofill::test::SetCreditCardInfo(credit_card2_.get(), "Jane Doe",
                                      "411111111111" /* Visa */, "01", "2999");
    personal_data_manager_->AddTestingCreditCard(credit_card2_.get());

    std::unique_ptr<web::PaymentRequest> web_payment_request =
        base::MakeUnique<web::PaymentRequest>();
    web::PaymentMethodData method_datum;
    method_datum.supported_methods.push_back(base::ASCIIToUTF16("visa"));
    web_payment_request->method_data.push_back(method_datum);

    payment_request_ = base::MakeUnique<PaymentRequest>(
        std::move(web_payment_request), personal_data_manager_.get());

    return [[PaymentMethodSelectionViewController alloc]
        initWithPaymentRequest:payment_request_.get()];
  }

  PaymentMethodSelectionViewController* PaymentMethodSelectionController() {
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
  CheckTitleWithId(IDS_IOS_PAYMENT_REQUEST_METHOD_SELECTION_TITLE);

  [PaymentMethodSelectionController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // One item for each of payment method plus 1 for the add button.
  EXPECT_EQ(3U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The first two items should be of type ShippingAddressItem. The first one
  // should appear to be selected.
  id item = GetCollectionViewItem(0, 0);
  ASSERT_TRUE([item isMemberOfClass:[PaymentMethodItem class]]);
  PaymentMethodItem* paymentMethodItem = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryCheckmark,
            paymentMethodItem.accessoryType);

  item = GetCollectionViewItem(0, 1);
  ASSERT_TRUE([item isMemberOfClass:[PaymentMethodItem class]]);
  paymentMethodItem = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone,
            paymentMethodItem.accessoryType);

  // The last item should also be of type TextAndMessageItem.
  item = GetCollectionViewItem(0, 2);
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
}

}  // namespace
