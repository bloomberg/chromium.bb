// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_items_display_view_controller.h"

#include <utility>
#include <vector>

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PaymentItemsDisplayViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    personal_data_manager_ =
        base::MakeUnique<autofill::TestPersonalDataManager>();

    std::unique_ptr<web::PaymentRequest> web_payment_request =
        base::MakeUnique<web::PaymentRequest>();
    web::PaymentItem payment_item;
    web_payment_request->details.display_items.push_back(payment_item);
    web_payment_request->details.display_items.push_back(payment_item);
    web_payment_request->details.display_items.push_back(payment_item);

    payment_request_ = base::MakeUnique<PaymentRequest>(
        std::move(web_payment_request), personal_data_manager_.get());

    return [[PaymentItemsDisplayViewController alloc]
        initWithPaymentRequest:payment_request_.get()
              payButtonEnabled:YES];
  }

  PaymentItemsDisplayViewController* PaymentItemsController() {
    return base::mac::ObjCCastStrict<PaymentItemsDisplayViewController>(
        controller());
  }

  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_manager_;
  std::unique_ptr<PaymentRequest> payment_request_;
};

// Tests that the correct number of items are displayed after loading the model.
TEST_F(PaymentItemsDisplayViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_IOS_PAYMENT_REQUEST_PAYMENT_ITEMS_TITLE);

  [PaymentItemsController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // There should be an item for each of the line items in paymentItems plus 1
  // for the total.
  EXPECT_EQ(4U, static_cast<unsigned int>(NumberOfItemsInSection(0)));
}

}  // namespace
