// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/credit_card_edit_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#import "ios/chrome/browser/payments/credit_card_edit_mediator.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_editor_field.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestCreditCardEditViewControllerMediator
    : CreditCardEditViewControllerMediator

@end

@implementation TestCreditCardEditViewControllerMediator

NSArray<EditorField*>* editorFields() {
  return @[
    [[EditorField alloc] initWithAutofillUIType:AutofillUITypeCreditCardNumber
                                          label:@"Credit Card Number"
                                          value:@"4111111111111111" /* Visa */
                                       required:YES],
    [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeCreditCardHolderFullName
                         label:@"Cardholder Name"
                         value:@"John Doe"
                      required:YES],
    [[EditorField alloc] initWithAutofillUIType:AutofillUITypeCreditCardExpMonth
                                          label:@"Expiration Month"
                                          value:@"12"
                                       required:YES],
    [[EditorField alloc] initWithAutofillUIType:AutofillUITypeCreditCardExpYear
                                          label:@"Expiration Year"
                                          value:@"2090"
                                       required:YES],
  ];
}

@end

class PaymentRequestCreditCardEditViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* InstantiateController() override {
    payment_request_ = base::MakeUnique<PaymentRequest>(
        payment_request_test_util::CreateTestWebPaymentRequest(),
        &personal_data_manager_);

    CreditCardEditViewControllerMediator* mediator =
        [[CreditCardEditViewControllerMediator alloc]
            initWithPaymentRequest:payment_request_.get()
                        creditCard:nil];

    CreditCardEditViewController* viewController =
        [[CreditCardEditViewController alloc] init];
    [viewController setDataSource:mediator];
    return viewController;
  }

  CreditCardEditViewController* GetCreditCardEditViewController() {
    return base::mac::ObjCCastStrict<CreditCardEditViewController>(
        controller());
  }

  autofill::TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<PaymentRequest> payment_request_;
};

// Tests that the correct number of items are displayed after loading the model.
TEST_F(PaymentRequestCreditCardEditViewControllerTest, TestModel) {
  CreateController();
  CheckController();

  [GetCreditCardEditViewController()
      setState:CreditCardEditViewControllerStateEdit];
  [GetCreditCardEditViewController() loadModel];

  // There is one section for every textfield (there are four textfields in
  // total), one for the footer, and one for the billing address ID item.
  ASSERT_EQ(6, NumberOfSections());

  // The first four sections have only one item of the type AutofillEditItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(0)));
  id item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[AutofillEditItem class]]);

  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(1)));
  item = GetCollectionViewItem(1, 0);
  EXPECT_TRUE([item isMemberOfClass:[AutofillEditItem class]]);

  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(2)));
  item = GetCollectionViewItem(2, 0);
  EXPECT_TRUE([item isMemberOfClass:[AutofillEditItem class]]);

  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(3)));
  item = GetCollectionViewItem(3, 0);
  EXPECT_TRUE([item isMemberOfClass:[AutofillEditItem class]]);

  // The billing address section contains one item which is of the type
  // CollectionViewDetailItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(4)));
  item = GetCollectionViewItem(4, 0);
  EXPECT_TRUE([item isMemberOfClass:[CollectionViewDetailItem class]]);
  CollectionViewDetailItem* billing_address_item = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            billing_address_item.accessoryType);

  // The footer section contains one item which is of the type
  // CollectionViewFooterItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(5)));
  item = GetCollectionViewItem(5, 0);
  EXPECT_TRUE([item isMemberOfClass:[CollectionViewFooterItem class]]);
}

// Tests that the correct number of items are displayed after loading the model,
// when creating a new credit card.
TEST_F(PaymentRequestCreditCardEditViewControllerTest,
       TestModelCreateNewCreditCard) {
  CreateController();
  CheckController();

  [GetCreditCardEditViewController()
      setState:CreditCardEditViewControllerStateCreate];
  [GetCreditCardEditViewController() loadModel];

  // There is an extra section containing a switch that allows the user to save
  // the credit card locally.
  ASSERT_EQ(7, NumberOfSections());

  // The switch section is the last section before the footer and has one item
  // of the type CollectionViewSwitchItem. The switch is on by defualt.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(5)));
  id item = GetCollectionViewItem(5, 0);
  EXPECT_TRUE([item isMemberOfClass:[CollectionViewSwitchItem class]]);
  CollectionViewSwitchItem* switch_item = item;
  EXPECT_EQ(YES, [switch_item isOn]);
}
