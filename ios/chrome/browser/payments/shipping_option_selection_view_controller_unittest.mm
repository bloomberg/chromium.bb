// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/shipping_option_selection_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/ui/autofill/cells/status_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ShippingOptionSelectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    personal_data_manager_ =
        base::MakeUnique<autofill::TestPersonalDataManager>();

    web::PaymentRequest web_payment_request =
        payment_request_test_util::CreateTestWebPaymentRequest();

    payment_request_ = base::MakeUnique<PaymentRequest>(
        base::MakeUnique<web::PaymentRequest>(web_payment_request),
        personal_data_manager_.get());

    return [[ShippingOptionSelectionViewController alloc]
        initWithPaymentRequest:payment_request_.get()];
  }

  ShippingOptionSelectionViewController*
  GetShippingOptionSelectionViewController() {
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
  CheckTitleWithId(IDS_PAYMENTS_SHIPPING_OPTION_LABEL);

  [GetShippingOptionSelectionViewController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // One item for each of shipping options.
  ASSERT_EQ(2U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The next two items should be of type CollectionViewTextItem. The first one
  // should appear to be selected.
  id item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[CollectionViewTextItem class]]);
  CollectionViewTextItem* text_item = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryCheckmark, text_item.accessoryType);

  item = GetCollectionViewItem(0, 1);
  EXPECT_TRUE([item isMemberOfClass:[CollectionViewTextItem class]]);
  text_item = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone, text_item.accessoryType);

  // Test the error state.
  [GetShippingOptionSelectionViewController() setErrorMessage:@"Lorem ipsum"];
  [GetShippingOptionSelectionViewController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // There should be 3 items now in total.
  ASSERT_EQ(3U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The first item should also be of type TextAndMessageItem.
  item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);

  // Test the pending state.
  [GetShippingOptionSelectionViewController() setPending:YES];
  [GetShippingOptionSelectionViewController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // There should be only one item.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The item should be of type StatusItem.
  item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[StatusItem class]]);
}
