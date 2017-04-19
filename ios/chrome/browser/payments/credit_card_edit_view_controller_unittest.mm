// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/credit_card_edit_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/field_types.h"
#import "ios/chrome/browser/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/payments/payment_request_editor_field.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/test_utils.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestCreditCardEditViewControllerMediator
    : NSObject<CreditCardEditViewControllerDataSource>

@end

@implementation TestCreditCardEditViewControllerMediator

- (CollectionViewItem*)serverCardSummaryItem {
  return [[PaymentMethodItem alloc] init];
}

- (NSArray<EditorField*>*)editorFields {
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

- (NSString*)billingAddressLabelForProfileWithGUID:(NSString*)profileGUID {
  return nil;
}

- (UIImage*)cardTypeIconFromCardNumber:(NSString*)cardNumber {
  return nil;
}

@end

class PaymentRequestCreditCardEditViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* InstantiateController() override {
    mediator_ = [[TestCreditCardEditViewControllerMediator alloc] init];

    CreditCardEditViewController* viewController =
        [[CreditCardEditViewController alloc] init];
    [viewController setDataSource:mediator_];
    return viewController;
  }

  CreditCardEditViewController* GetCreditCardEditViewController() {
    return base::mac::ObjCCastStrict<CreditCardEditViewController>(
        controller());
  }

  TestCreditCardEditViewControllerMediator* mediator_ = nil;
};

// Tests that the correct number of items are displayed after loading the model.
TEST_F(PaymentRequestCreditCardEditViewControllerTest, TestModel) {
  CreateController();
  CheckController();

  [GetCreditCardEditViewController()
      setState:CreditCardEditViewControllerStateEdit];
  [GetCreditCardEditViewController() loadModel];

  // There is one section for every textfield (there are four textfields in
  // total), one for the server card summary section, one for the footer, and
  // one for the billing address ID item.
  ASSERT_EQ(7, NumberOfSections());

  // The server card summary section is the first section and has one item of
  // the type PaymentMethodItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(0)));
  id item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[PaymentMethodItem class]]);

  // The next four sections have only one item of the type AutofillEditItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(1)));
  item = GetCollectionViewItem(1, 0);
  EXPECT_TRUE([item isMemberOfClass:[AutofillEditItem class]]);

  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(2)));
  item = GetCollectionViewItem(2, 0);
  EXPECT_TRUE([item isMemberOfClass:[AutofillEditItem class]]);

  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(3)));
  item = GetCollectionViewItem(3, 0);
  EXPECT_TRUE([item isMemberOfClass:[AutofillEditItem class]]);

  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(4)));
  item = GetCollectionViewItem(4, 0);
  EXPECT_TRUE([item isMemberOfClass:[AutofillEditItem class]]);

  // The billing address section contains one item which is of the type
  // CollectionViewDetailItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(5)));
  item = GetCollectionViewItem(5, 0);
  EXPECT_TRUE([item isMemberOfClass:[CollectionViewDetailItem class]]);
  CollectionViewDetailItem* billing_address_item = item;
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            billing_address_item.accessoryType);

  // The footer section contains one item which is of the type
  // CollectionViewFooterItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(6)));
  item = GetCollectionViewItem(6, 0);
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
  ASSERT_EQ(8, NumberOfSections());

  // The switch section is the last section before the footer and has one item
  // of the type CollectionViewSwitchItem. The switch is on by defualt.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(6)));
  id item = GetCollectionViewItem(6, 0);
  EXPECT_TRUE([item isMemberOfClass:[CollectionViewSwitchItem class]]);
  CollectionViewSwitchItem* switch_item = item;
  EXPECT_EQ(YES, [switch_item isOn]);
}
