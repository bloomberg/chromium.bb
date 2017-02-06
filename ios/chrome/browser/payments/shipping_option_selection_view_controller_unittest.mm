// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/shipping_option_selection_view_controller.h"

#include <utility>
#include <vector>

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/autofill/cells/status_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ShippingOptionSelectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    personal_data_manager_ =
        base::MakeUnique<autofill::TestPersonalDataManager>();

    std::unique_ptr<web::PaymentRequest> web_payment_request =
        base::MakeUnique<web::PaymentRequest>();
    web::PaymentShippingOption shipping_option1;
    shipping_option1.id = base::ASCIIToUTF16("1234");
    shipping_option1.label = base::ASCIIToUTF16("1-Day");
    shipping_option1.amount.value = base::ASCIIToUTF16("0.99");
    shipping_option1.amount.currency = base::ASCIIToUTF16("USD");
    shipping_option1.selected = true;
    web_payment_request->details.shipping_options.push_back(shipping_option1);
    web::PaymentShippingOption shipping_option2;
    shipping_option2.id = base::ASCIIToUTF16("4321");
    shipping_option2.label = base::ASCIIToUTF16("10-Days");
    shipping_option2.amount.value = base::ASCIIToUTF16("0.01");
    shipping_option2.amount.currency = base::ASCIIToUTF16("USD");
    shipping_option2.selected = false;
    web_payment_request->details.shipping_options.push_back(shipping_option2);
    web_payment_request->options.request_shipping = true;

    payment_request_ = base::MakeUnique<PaymentRequest>(
        std::move(web_payment_request), personal_data_manager_.get());

    return [[ShippingOptionSelectionViewController alloc]
        initWithPaymentRequest:payment_request_.get()];
  }

  ShippingOptionSelectionViewController* ShippingOptionSelectionController() {
    return base::mac::ObjCCastStrict<ShippingOptionSelectionViewController>(
        controller());
  }

  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_manager_;
  std::unique_ptr<PaymentRequest> payment_request_;
};

// Tests that the correct number of items are displayed after loading the model
// and that the correct item appears to be selected.
TEST_F(ShippingOptionSelectionViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_IOS_PAYMENT_REQUEST_SHIPPING_OPTION_SELECTION_TITLE);

  [ShippingOptionSelectionController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // One item for each of shipping options.
  EXPECT_EQ(2U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The next two items should be of type CollectionViewTextItem. The first one
  // should appear to be selected.
  id item = GetCollectionViewItem(0, 0);
  ASSERT_TRUE([item isMemberOfClass:[CollectionViewTextItem class]]);
  CollectionViewTextItem* textItem = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryCheckmark, textItem.accessoryType);

  item = GetCollectionViewItem(0, 1);
  ASSERT_TRUE([item isMemberOfClass:[CollectionViewTextItem class]]);
  textItem = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone, textItem.accessoryType);

  // Test the error state.
  [ShippingOptionSelectionController() setErrorMessage:@"Lorem ipsum"];
  [ShippingOptionSelectionController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // There should be 3 items now in total.
  EXPECT_EQ(3U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The first item should also be of type TextAndMessageItem.
  item = GetCollectionViewItem(0, 0);
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);

  // Test the loading state.
  [ShippingOptionSelectionController() setIsLoading:YES];
  [ShippingOptionSelectionController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // There should be only one item.
  EXPECT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The item should be of type StatusItem.
  item = GetCollectionViewItem(0, 0);
  ASSERT_TRUE([item isMemberOfClass:[StatusItem class]]);
}

}  // namespace
