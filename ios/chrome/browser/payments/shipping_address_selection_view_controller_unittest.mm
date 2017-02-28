// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/shipping_address_selection_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/cells/autofill_profile_item.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/ui/autofill/cells/status_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ShippingAddressSelectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    personal_data_manager_ =
        base::MakeUnique<autofill::TestPersonalDataManager>();
    // Add testing profiles. autofill::TestPersonalDataManager does not take
    // ownership of the profiles.
    profile1_ = payment_request_test_util::CreateTestAutofillProfile();
    personal_data_manager_->AddTestingProfile(profile1_.get());
    profile2_ = payment_request_test_util::CreateTestAutofillProfile();
    personal_data_manager_->AddTestingProfile(profile2_.get());

    web::PaymentRequest web_payment_request =
        payment_request_test_util::CreateTestWebPaymentRequest();

    payment_request_ = base::MakeUnique<PaymentRequest>(
        base::MakeUnique<web::PaymentRequest>(web_payment_request),
        personal_data_manager_.get());

    return [[ShippingAddressSelectionViewController alloc]
        initWithPaymentRequest:payment_request_.get()];
  }

  ShippingAddressSelectionViewController*
  GetShippingAddressSelectionViewController() {
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
  CheckTitleWithId(IDS_PAYMENTS_SHIPPING_ADDRESS_LABEL);

  [GetShippingAddressSelectionViewController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // One item for each of shipping addresses plus 2 for the message and the add
  // button.
  ASSERT_EQ(4U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The first item should be of type TextAndMessageItem.
  id item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);

  // The next two items should be of type AutofillProfileItem. The first one
  // should appear to be selected.
  item = GetCollectionViewItem(0, 1);
  ASSERT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* shipping_address_item = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryCheckmark,
            shipping_address_item.accessoryType);

  item = GetCollectionViewItem(0, 2);
  EXPECT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);
  shipping_address_item = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone,
            shipping_address_item.accessoryType);

  // The last item should also be of type TextAndMessageItem.
  item = GetCollectionViewItem(0, 3);
  EXPECT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);

  // Test the error state.
  [GetShippingAddressSelectionViewController() setErrorMessage:@"Lorem ipsum"];
  [GetShippingAddressSelectionViewController() loadModel];
  ASSERT_EQ(1, NumberOfSections());
  // There should stil be 4 items in total.
  ASSERT_EQ(4U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // Test the pending state.
  [GetShippingAddressSelectionViewController() setPending:YES];
  [GetShippingAddressSelectionViewController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // There should be only one item.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The item should be of type StatusItem.
  item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[StatusItem class]]);
}
