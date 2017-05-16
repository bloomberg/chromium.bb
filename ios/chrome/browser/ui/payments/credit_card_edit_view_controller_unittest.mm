// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/credit_card_edit_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/test_utils.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/payments/cells/accepted_payment_methods_item.h"
#import "ios/chrome/browser/ui/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_selector_edit_item.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_consumer.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestCreditCardEditViewControllerMediator
    : NSObject<CreditCardEditViewControllerDataSource>

@property(nonatomic, weak) id<PaymentRequestEditConsumer> consumer;

@end

@implementation TestCreditCardEditViewControllerMediator

@synthesize state = _state;
@synthesize consumer = _consumer;

- (CollectionViewItem*)headerItem {
  return [[PaymentMethodItem alloc] init];
}

- (BOOL)shouldHideBackgroundForHeaderItem {
  return NO;
}

- (void)setConsumer:(id<PaymentRequestEditConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setEditorFields:@[
    [[EditorField alloc] initWithAutofillUIType:AutofillUITypeCreditCardNumber
                                      fieldType:EditorFieldTypeTextField
                                          label:@"Credit Card Number"
                                          value:@"4111111111111111" /* Visa */
                                       required:YES],
    [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeCreditCardHolderFullName
                     fieldType:EditorFieldTypeTextField
                         label:@"Cardholder Name"
                         value:@"John Doe"
                      required:YES],
    [[EditorField alloc] initWithAutofillUIType:AutofillUITypeCreditCardExpMonth
                                      fieldType:EditorFieldTypeTextField
                                          label:@"Expiration Month"
                                          value:@"12"
                                       required:YES],
    [[EditorField alloc] initWithAutofillUIType:AutofillUITypeCreditCardExpYear
                                      fieldType:EditorFieldTypeTextField
                                          label:@"Expiration Year"
                                          value:@"2090"
                                       required:YES],
    [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeCreditCardBillingAddress
                     fieldType:EditorFieldTypeSelector
                         label:@"Billing Address"
                         value:@"12345"
                      required:YES],
  ]];
}

- (UIImage*)cardTypeIconFromCardNumber:(NSString*)cardNumber {
  return nil;
}

@end

class PaymentRequestCreditCardEditViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* InstantiateController() override {
    CreditCardEditViewController* viewController =
        [[CreditCardEditViewController alloc] init];
    mediator_ = [[TestCreditCardEditViewControllerMediator alloc] init];
    [mediator_ setConsumer:viewController];
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

  [mediator_ setState:EditViewControllerStateEdit];
  [GetCreditCardEditViewController() loadModel];

  // There is one section containing the credit card type icons for the accepted
  // payment methods. In addition to that, there is one section for every field
  // (there are five form fields in total), and finally one for the footer.
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
  // PaymentsSelectorEditItem.
  ASSERT_EQ(1U, static_cast<unsigned int>(NumberOfItemsInSection(5)));
  item = GetCollectionViewItem(5, 0);
  EXPECT_TRUE([item isMemberOfClass:[PaymentsSelectorEditItem class]]);
  PaymentsSelectorEditItem* billing_address_item = item;
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

  [mediator_ setState:EditViewControllerStateCreate];
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
