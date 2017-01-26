// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_method_selection_view_controller.h"

#include <vector>

#include "base/mac/foundation_util.h"
#include "components/autofill/core/browser/credit_card.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PaymentMethodSelectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    return [[PaymentMethodSelectionViewController alloc] init];
  }

  PaymentMethodSelectionViewController* PaymentMethodSelectionController() {
    return base::mac::ObjCCastStrict<PaymentMethodSelectionViewController>(
        controller());
  }
};

// Tests that the correct number of items are displayed after loading the model
// and that the correct item appears to be selected.
TEST_F(PaymentMethodSelectionViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_IOS_PAYMENT_REQUEST_METHOD_SELECTION_TITLE);

  std::unique_ptr<autofill::CreditCard> creditCard1(new autofill::CreditCard());
  std::unique_ptr<autofill::CreditCard> creditCard2(new autofill::CreditCard());

  std::vector<autofill::CreditCard*> creditCards;
  creditCards.push_back(creditCard1.get());
  creditCards.push_back(creditCard2.get());

  [PaymentMethodSelectionController() setPaymentMethods:creditCards];
  [PaymentMethodSelectionController() setSelectedPaymentMethod:creditCards[0]];
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
