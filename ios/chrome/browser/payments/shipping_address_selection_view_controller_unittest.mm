// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/shipping_address_selection_view_controller.h"

#include <vector>

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/payments/cells/shipping_address_item.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/autofill/cells/status_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ShippingAddressSelectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    personal_data_manager_ =
        base::MakeUnique<autofill::TestPersonalDataManager>();
    // AddTestingProfile will not take ownership of the profiles passed to it.
    profile1_ = base::MakeUnique<autofill::AutofillProfile>();
    personal_data_manager_->AddTestingProfile(profile1_.get());
    profile2_ = base::MakeUnique<autofill::AutofillProfile>();
    personal_data_manager_->AddTestingProfile(profile2_.get());

    payment_request_ = base::MakeUnique<PaymentRequest>(
        base::MakeUnique<web::PaymentRequest>(), personal_data_manager_.get());

    return [[ShippingAddressSelectionViewController alloc]
        initWithPaymentRequest:payment_request_.get()];
  }

  ShippingAddressSelectionViewController* ShippingAddressSelectionController() {
    return base::mac::ObjCCastStrict<ShippingAddressSelectionViewController>(
        controller());
  }

  std::unique_ptr<autofill::AutofillProfile> profile1_;
  std::unique_ptr<autofill::AutofillProfile> profile2_;
  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_manager_;
  std::unique_ptr<PaymentRequest> payment_request_;
};

// Tests that the correct number of items are displayed after loading the model
// and that the correct item appears to be selected.
TEST_F(ShippingAddressSelectionViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_IOS_PAYMENT_REQUEST_SHIPPING_ADDRESS_SELECTION_TITLE);

  [ShippingAddressSelectionController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // One item for each of shipping addresses plus 2 for the message and the add
  // button.
  EXPECT_EQ(4U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The first item should be of type TextAndMessageItem.
  id item = GetCollectionViewItem(0, 0);
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);

  // The next two items should be of type ShippingAddressItem. The first one
  // should appear to be selected.
  item = GetCollectionViewItem(0, 1);
  ASSERT_TRUE([item isMemberOfClass:[ShippingAddressItem class]]);
  ShippingAddressItem* shippingAddressItem = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryCheckmark,
            shippingAddressItem.accessoryType);

  item = GetCollectionViewItem(0, 2);
  ASSERT_TRUE([item isMemberOfClass:[ShippingAddressItem class]]);
  shippingAddressItem = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone,
            shippingAddressItem.accessoryType);

  // The last item should also be of type TextAndMessageItem.
  item = GetCollectionViewItem(0, 3);
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);

  // Test the error state.
  [ShippingAddressSelectionController() setErrorMessage:@"Lorem ipsum"];
  [ShippingAddressSelectionController() loadModel];
  ASSERT_EQ(1, NumberOfSections());
  // There should stil be 4 items in total.
  EXPECT_EQ(4U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // Test the loading state.
  [ShippingAddressSelectionController() setIsLoading:YES];
  [ShippingAddressSelectionController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // There should be only one item.
  EXPECT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The item should be of type StatusItem.
  item = GetCollectionViewItem(0, 0);
  ASSERT_TRUE([item isMemberOfClass:[StatusItem class]]);
}

}  // namespace
