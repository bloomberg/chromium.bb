// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_items_display_view_controller.h"

#include <vector>

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PaymentItemsDisplayViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    return [[PaymentItemsDisplayViewController alloc]
        initWithPayButtonEnabled:YES];
  }

  PaymentItemsDisplayViewController* PaymentItemsController() {
    return base::mac::ObjCCastStrict<PaymentItemsDisplayViewController>(
        controller());
  }
};

// Tests that the correct number of items are displayed after loading the model.
TEST_F(PaymentItemsDisplayViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_IOS_PAYMENT_REQUEST_PAYMENT_ITEMS_TITLE);

  std::vector<web::PaymentItem> paymentItems;
  web::PaymentItem paymentItem;
  paymentItems.push_back(paymentItem);
  paymentItems.push_back(paymentItem);
  paymentItems.push_back(paymentItem);

  [PaymentItemsController() setTotal:paymentItem];
  [PaymentItemsController() setPaymentItems:paymentItems];
  [PaymentItemsController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // There should be an item for each of the line items in paymentItems plus 1
  // for the total.
  EXPECT_EQ(paymentItems.size() + 1,
            static_cast<unsigned int>(NumberOfItemsInSection(0)));
}

}  // namespace
