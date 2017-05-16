// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/credit_card_edit_coordinator.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/payments/credit_card_edit_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
class MockTestPersonalDataManager : public autofill::TestPersonalDataManager {
 public:
  MockTestPersonalDataManager() : TestPersonalDataManager() {}
  MOCK_METHOD1(AddCreditCard, void(const autofill::CreditCard&));
};

class MockPaymentRequest : public PaymentRequest {
 public:
  MockPaymentRequest(web::PaymentRequest web_payment_request,
                     autofill::PersonalDataManager* personal_data_manager)
      : PaymentRequest(web_payment_request, personal_data_manager) {}
  MOCK_METHOD1(AddCreditCard,
               autofill::CreditCard*(const autofill::CreditCard&));
};

MATCHER_P5(CreditCardMatches,
           credit_card_number,
           cardholder_name,
           expiration_month,
           expiration_year,
           billing_address_id,
           "") {
  return arg.GetRawInfo(autofill::CREDIT_CARD_NUMBER) ==
             base::ASCIIToUTF16(credit_card_number) &&
         arg.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL) ==
             base::ASCIIToUTF16(cardholder_name) &&
         arg.GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH) ==
             base::ASCIIToUTF16(expiration_month) &&
         arg.GetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR) ==
             base::ASCIIToUTF16(expiration_year) &&
         arg.billing_address_id() == billing_address_id;
}

NSArray<EditorField*>* GetEditorFields() {
  return @[
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
  ];
}

using ::testing::_;
}  // namespace

class PaymentRequestCreditCardEditCoordinatorTest : public PlatformTest {
 protected:
  PaymentRequestCreditCardEditCoordinatorTest() {
    payment_request_ = base::MakeUnique<MockPaymentRequest>(
        payment_request_test_util::CreateTestWebPaymentRequest(),
        &personal_data_manager_);
  }

  MockTestPersonalDataManager personal_data_manager_;
  std::unique_ptr<MockPaymentRequest> payment_request_;
};

// Tests that invoking start and stop on the coordinator presents and dismisses
// the credit card edit view controller, respectively.
TEST_F(PaymentRequestCreditCardEditCoordinatorTest, StartAndStop) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  CreditCardEditCoordinator* coordinator = [[CreditCardEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  EXPECT_EQ(1u, navigation_controller.viewControllers.count);

  [coordinator start];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, navigation_controller.viewControllers.count);

  [coordinator stop];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(1u, navigation_controller.viewControllers.count);
}

// Tests that calling the view controller delegate method which signals that the
// user has finished creating a new credit card, causes the credit card to be
// added to the PaymentRequest instance and the corresponding coordinator
// delegate method to get called. The new credit card is expected to get added
// to the PersonalDataManager if user chooses to save it locally.
TEST_F(PaymentRequestCreditCardEditCoordinatorTest, DidFinishCreatingWithSave) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  CreditCardEditCoordinator* coordinator = [[CreditCardEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(CreditCardEditCoordinatorDelegate)];
  [[delegate expect]
       creditCardEditCoordinator:coordinator
      didFinishEditingCreditCard:static_cast<autofill::CreditCard*>(
                                     [OCMArg anyPointer])];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(1u, navigation_controller.viewControllers.count);

  [coordinator start];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, navigation_controller.viewControllers.count);

  // Expect a credit card to be added to the PaymentRequest.
  EXPECT_CALL(*payment_request_,
              AddCreditCard(CreditCardMatches("4111111111111111", "John Doe",
                                              "12", "2090", "12345")))
      .Times(1);
  // Expect a credit card to be added to the PersonalDataManager.
  EXPECT_CALL(personal_data_manager_,
              AddCreditCard(CreditCardMatches("4111111111111111", "John Doe",
                                              "12", "2090", "12345")))
      .Times(1);

  // Call the controller delegate method.
  CreditCardEditViewController* view_controller =
      base::mac::ObjCCastStrict<CreditCardEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator creditCardEditViewController:view_controller
                     didFinishEditingFields:GetEditorFields()
                             saveCreditCard:YES];

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that calling the view controller delegate method which signals that the
// user has finished creating a new credit card, causes the credit card to be
// added to the PaymentRequest instance and the corresponding coordinator
// delegate method to get called. The new credit card should not get added to
// the PersonalDataManager if user chooses to not to save it locally.
TEST_F(PaymentRequestCreditCardEditCoordinatorTest, DidFinishCreatingNoSave) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  CreditCardEditCoordinator* coordinator = [[CreditCardEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(CreditCardEditCoordinatorDelegate)];
  [[delegate expect]
       creditCardEditCoordinator:coordinator
      didFinishEditingCreditCard:static_cast<autofill::CreditCard*>(
                                     [OCMArg anyPointer])];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(1u, navigation_controller.viewControllers.count);

  [coordinator start];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, navigation_controller.viewControllers.count);

  // Expect a credit card to be added to the PaymentRequest.
  EXPECT_CALL(*payment_request_,
              AddCreditCard(CreditCardMatches("4111111111111111", "John Doe",
                                              "12", "2090", "12345")))
      .Times(1);
  // No credit card should get added to the PersonalDataManager.
  EXPECT_CALL(personal_data_manager_, AddCreditCard(_)).Times(0);

  // Call the controller delegate method.
  CreditCardEditViewController* view_controller =
      base::mac::ObjCCastStrict<CreditCardEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator creditCardEditViewController:view_controller
                     didFinishEditingFields:GetEditorFields()
                             saveCreditCard:NO];

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that calling the view controller delegate method which signals that the
// user has finished editing a credit card, causes the corresponding coordinator
// delegate method to get called. The credit card should not get re-added to the
// PaymentRequest nor the PersonalDataManager.
TEST_F(PaymentRequestCreditCardEditCoordinatorTest, DidFinishEditing) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  CreditCardEditCoordinator* coordinator = [[CreditCardEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Set the credit card to be edited.
  autofill::CreditCard credit_card;
  [coordinator setCreditCard:&credit_card];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(CreditCardEditCoordinatorDelegate)];
  [[delegate expect]
       creditCardEditCoordinator:coordinator
      didFinishEditingCreditCard:static_cast<autofill::CreditCard*>(
                                     [OCMArg anyPointer])];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(1u, navigation_controller.viewControllers.count);

  [coordinator start];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, navigation_controller.viewControllers.count);

  // No credit card should get added to the PaymentRequest.
  EXPECT_CALL(*payment_request_, AddCreditCard(_)).Times(0);
  // No credit card should get added to the PersonalDataManager.
  EXPECT_CALL(personal_data_manager_, AddCreditCard(_)).Times(0);

  // Call the controller delegate method.
  CreditCardEditViewController* view_controller =
      base::mac::ObjCCastStrict<CreditCardEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator creditCardEditViewController:view_controller
                     didFinishEditingFields:GetEditorFields()
                             saveCreditCard:YES];

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that calling the view controller delegate method which signals that the
// user has chosen to cancel creating/editing a credit card, causes the
// corresponding coordinator delegate method to get called.
TEST_F(PaymentRequestCreditCardEditCoordinatorTest, DidCancel) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  CreditCardEditCoordinator* coordinator = [[CreditCardEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(CreditCardEditCoordinatorDelegate)];
  [[delegate expect] creditCardEditCoordinatorDidCancel:coordinator];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(1u, navigation_controller.viewControllers.count);

  [coordinator start];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, navigation_controller.viewControllers.count);

  // Call the controller delegate method.
  CreditCardEditViewController* view_controller =
      base::mac::ObjCCastStrict<CreditCardEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator creditCardEditViewControllerDidCancel:view_controller];

  EXPECT_OCMOCK_VERIFY(delegate);
}
