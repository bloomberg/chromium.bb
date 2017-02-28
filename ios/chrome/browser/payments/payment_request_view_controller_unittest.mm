// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_request_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/payments/cells/autofill_profile_item.h"
#import "ios/chrome/browser/payments/cells/page_info_item.h"
#import "ios/chrome/browser/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/payments/cells/price_item.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/ui/autofill/cells/status_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PaymentRequestViewControllerTest : public CollectionViewControllerTest {
 protected:
  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    personal_data_manager_ =
        base::MakeUnique<autofill::TestPersonalDataManager>();
    // Add testing profile. autofill::TestPersonalDataManager does not take
    // ownership of the profile.
    profile_ = payment_request_test_util::CreateTestAutofillProfile();
    personal_data_manager_->AddTestingProfile(profile_.get());
    // Add testing credit card. autofill::TestPersonalDataManager does not take
    // ownership of the card.
    credit_card_ = payment_request_test_util::CreateTestCreditCard();
    personal_data_manager_->AddTestingCreditCard(credit_card_.get());

    web::PaymentRequest web_payment_request =
        payment_request_test_util::CreateTestWebPaymentRequest();

    payment_request_ = base::MakeUnique<PaymentRequest>(
        base::MakeUnique<web::PaymentRequest>(web_payment_request),
        personal_data_manager_.get());

    return [[PaymentRequestViewController alloc]
        initWithPaymentRequest:payment_request_.get()];
  }

  PaymentRequestViewController* GetPaymentRequestViewController() {
    return base::mac::ObjCCastStrict<PaymentRequestViewController>(
        controller());
  }

  std::unique_ptr<autofill::AutofillProfile> profile_;
  std::unique_ptr<autofill::CreditCard> credit_card_;
  std::unique_ptr<PaymentRequest> payment_request_;
  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_manager_;
};

// Tests that the correct items are displayed after loading the model.
TEST_F(PaymentRequestViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_PAYMENTS_TITLE);

  [GetPaymentRequestViewController() loadModel];

  // There should be three sections in total. Summary, Shipping, and Payment.
  ASSERT_EQ(4, NumberOfSections());

  // The only item in the Summary section should be of type PriceItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(0)));
  id item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[PriceItem class]]);
  PriceItem* price_item = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            price_item.accessoryType);

  // There should be two items in the Shipping section.
  ASSERT_EQ(2U, static_cast<unsigned int>(NumberOfItemsInSection(1)));

  // The first one should be of type AutofillProfileItem.
  item = GetCollectionViewItem(1, 0);
  EXPECT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* shipping_address_item = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            shipping_address_item.accessoryType);

  // The next item should be of type CollectionViewTextItem.
  item = GetCollectionViewItem(1, 1);
  EXPECT_TRUE([item isMemberOfClass:[CollectionViewTextItem class]]);
  CollectionViewTextItem* shipping_option_item = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            shipping_option_item.accessoryType);

  // The only item in the Payment section should be of type PaymentMethodItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(2)));
  item = GetCollectionViewItem(2, 0);
  EXPECT_TRUE([item isMemberOfClass:[PaymentMethodItem class]]);

  // The only item in the Contact info section should be of type
  // AutofillProfileItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(3)));
  item = GetCollectionViewItem(3, 0);
  EXPECT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);
}

// Tests that the correct items are displayed after loading the model, when
// there are no display items.
TEST_F(PaymentRequestViewControllerTest, TestModelNoDisplayItem) {
  CreateController();
  CheckController();

  payment_request_->set_payment_details(web::PaymentDetails());
  [GetPaymentRequestViewController() loadModel];

  // The only item in the Summary section should stil be of type PriceItem, but
  // without an accessory view.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(0)));
  id item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[PriceItem class]]);
  PriceItem* price_item = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone, price_item.accessoryType);
}

// Tests that the correct items are displayed after loading the model, when
// there is no selected shipping addresse.
TEST_F(PaymentRequestViewControllerTest, TestModelNoSelectedShippingAddress) {
  CreateController();
  CheckController();

  payment_request_->set_selected_shipping_profile(nullptr);
  [GetPaymentRequestViewController() loadModel];

  // There should still be two items in the Shipping section.
  ASSERT_EQ(2U, static_cast<unsigned int>(NumberOfItemsInSection(1)));

  // The first one should be of type CollectionViewDetailItem.
  id item = GetCollectionViewItem(1, 0);
  EXPECT_TRUE([item isMemberOfClass:[CollectionViewDetailItem class]]);
  CollectionViewDetailItem* detail_item = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone, detail_item.accessoryType);
}

// Tests that the correct items are displayed after loading the model, when
// there is no selected shipping option.
TEST_F(PaymentRequestViewControllerTest, TestModelNoSelectedShippingOption) {
  CreateController();
  CheckController();

  // Resetting the payment details should reset the selected shipping option.
  payment_request_->set_payment_details(web::PaymentDetails());
  [GetPaymentRequestViewController() loadModel];

  // There should still be two items in the Shipping section.
  ASSERT_EQ(2U, static_cast<unsigned int>(NumberOfItemsInSection(1)));

  // The second one should be of type CollectionViewDetailItem.
  id item = GetCollectionViewItem(1, 1);
  EXPECT_TRUE([item isMemberOfClass:[CollectionViewDetailItem class]]);
  CollectionViewDetailItem* detail_item = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            detail_item.accessoryType);
}

// Tests that the correct items are displayed after loading the model, when
// there is no selected payment method.
TEST_F(PaymentRequestViewControllerTest, TestModelNoSelectedPaymentMethod) {
  CreateController();
  CheckController();

  payment_request_->set_selected_credit_card(nullptr);
  [GetPaymentRequestViewController() loadModel];

  // The only item in the Payment section should be of type
  // CollectionViewDetailItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(2)));
  id item = GetCollectionViewItem(2, 0);
  EXPECT_TRUE([item isMemberOfClass:[CollectionViewDetailItem class]]);
  CollectionViewDetailItem* detail_item = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone, detail_item.accessoryType);
}

// Tests that the correct items are displayed after loading the model, when
// the view is in pending state.
TEST_F(PaymentRequestViewControllerTest, TestModelPendingState) {
  CreateController();
  CheckController();

  [GetPaymentRequestViewController() setPending:YES];
  [GetPaymentRequestViewController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // There should be only one item.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The item should be of type StatusItem.
  id item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[StatusItem class]]);
}
