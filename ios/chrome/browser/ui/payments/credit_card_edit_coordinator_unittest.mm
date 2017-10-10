// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/credit_card_edit_coordinator.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/core/autofill_payment_instrument.h"
#include "components/payments/core/payment_instrument.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#include "ios/chrome/browser/payments/test_payment_request.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#import "ios/chrome/browser/ui/payments/payment_request_navigation_controller.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/test_web_state.h"
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
  MOCK_METHOD1(UpdateCreditCard, void(const autofill::CreditCard&));
};

class MockPaymentRequest : public payments::TestPaymentRequest {
 public:
  MockPaymentRequest(payments::WebPaymentRequest web_payment_request,
                     ios::ChromeBrowserState* browser_state,
                     web::WebState* web_state,
                     autofill::PersonalDataManager* personal_data_manager)
      : payments::TestPaymentRequest(web_payment_request,
                                     browser_state,
                                     web_state,
                                     personal_data_manager) {}
  MOCK_METHOD1(
      AddAutofillPaymentInstrument,
      payments::AutofillPaymentInstrument*(const autofill::CreditCard&));
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

NSArray<EditorField*>* GetEditorFields(bool save_card) {
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
    [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeCreditCardSaveToChrome
                     fieldType:EditorFieldTypeSwitch
                         label:@"Save Card"
                         value:save_card ? @"YES" : @"NO"
                      required:YES],
  ];
}

using ::testing::_;
}  // namespace

class PaymentRequestCreditCardEditCoordinatorTest : public PlatformTest {
 protected:
  PaymentRequestCreditCardEditCoordinatorTest()
      : chrome_browser_state_(TestChromeBrowserState::Builder().Build()) {
    payment_request_ = base::MakeUnique<MockPaymentRequest>(
        payment_request_test_util::CreateTestWebPaymentRequest(),
        chrome_browser_state_.get(), &web_state_, &personal_data_manager_);
  }

  base::test::ScopedTaskEnvironment scoped_task_evironment_;

  web::TestWebState web_state_;
  MockTestPersonalDataManager personal_data_manager_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<MockPaymentRequest> payment_request_;
};

// Tests that invoking start and stop on the coordinator presents and dismisses
// the credit card edit view controller, respectively.
TEST_F(PaymentRequestCreditCardEditCoordinatorTest, StartAndStop) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  CreditCardEditCoordinator* coordinator = [[CreditCardEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  EXPECT_EQ(nil, base_view_controller.presentedViewController);

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_TRUE([base_view_controller.presentedViewController
      isMemberOfClass:[PaymentRequestNavigationController class]]);
  PaymentRequestNavigationController* navigation_controller =
      base::mac::ObjCCastStrict<PaymentRequestNavigationController>(
          base_view_controller.presentedViewController);
  EXPECT_TRUE([navigation_controller.visibleViewController
      isMemberOfClass:[PaymentRequestEditViewController class]]);

  [coordinator stop];
  // Wait until the animation completes and the presented view controller is
  // dismissed.
  base::test::ios::WaitUntilCondition(^bool() {
    return !base_view_controller.presentedViewController;
  });
  EXPECT_EQ(nil, base_view_controller.presentedViewController);
}

// Tests that calling the view controller delegate method which signals that the
// user has finished creating a new payment method, causes the payment method to
// be added to the PaymentRequest instance and the corresponding coordinator
// delegate method to get called. The new payment method is expected to get
// added to the PersonalDataManager if user chooses to save it locally.
TEST_F(PaymentRequestCreditCardEditCoordinatorTest, DidFinishCreatingWithSave) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  CreditCardEditCoordinator* coordinator = [[CreditCardEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(CreditCardEditCoordinatorDelegate)];
  [[delegate expect]
          creditCardEditCoordinator:coordinator
      didFinishEditingPaymentMethod:static_cast<
                                        payments::AutofillPaymentInstrument*>(
                                        [OCMArg anyPointer])];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(nil, base_view_controller.presentedViewController);

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_NE(nil, base_view_controller.presentedViewController);

  // Expect a payment method to be added to the PaymentRequest.
  EXPECT_CALL(*payment_request_,
              AddAutofillPaymentInstrument(CreditCardMatches(
                  "4111111111111111", "John Doe", "12", "2090", "12345")))
      .Times(1);
  // Expect a payment method to be added to the PersonalDataManager.
  EXPECT_CALL(personal_data_manager_,
              AddCreditCard(CreditCardMatches("4111111111111111", "John Doe",
                                              "12", "2090", "12345")))
      .Times(1);
  // No payment method should get updated in the PersonalDataManager.
  EXPECT_CALL(personal_data_manager_, UpdateCreditCard(_)).Times(0);

  // Call the controller delegate method.
  EXPECT_TRUE([base_view_controller.presentedViewController
      isMemberOfClass:[PaymentRequestNavigationController class]]);
  PaymentRequestNavigationController* navigation_controller =
      base::mac::ObjCCastStrict<PaymentRequestNavigationController>(
          base_view_controller.presentedViewController);
  PaymentRequestEditViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator paymentRequestEditViewController:view_controller
                         didFinishEditingFields:GetEditorFields(true)];

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that calling the view controller delegate method which signals that the
// user has finished creating a new payment method, causes the payment method to
// be added to the PaymentRequest instance and the corresponding coordinator
// delegate method to get called. The new payment method should not get added to
// the PersonalDataManager if user chooses to not to save it locally.
TEST_F(PaymentRequestCreditCardEditCoordinatorTest, DidFinishCreatingNoSave) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  CreditCardEditCoordinator* coordinator = [[CreditCardEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(CreditCardEditCoordinatorDelegate)];
  [[delegate expect]
          creditCardEditCoordinator:coordinator
      didFinishEditingPaymentMethod:static_cast<
                                        payments::AutofillPaymentInstrument*>(
                                        [OCMArg anyPointer])];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(nil, base_view_controller.presentedViewController);

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_NE(nil, base_view_controller.presentedViewController);

  // Expect a payment method to be added to the PaymentRequest.
  EXPECT_CALL(*payment_request_,
              AddAutofillPaymentInstrument(CreditCardMatches(
                  "4111111111111111", "John Doe", "12", "2090", "12345")))
      .Times(1);
  // No payment method should get added to the PersonalDataManager.
  EXPECT_CALL(personal_data_manager_, AddCreditCard(_)).Times(0);
  // No payment method should get updated in the PersonalDataManager.
  EXPECT_CALL(personal_data_manager_, UpdateCreditCard(_)).Times(0);

  // Call the controller delegate method.
  EXPECT_TRUE([base_view_controller.presentedViewController
      isMemberOfClass:[PaymentRequestNavigationController class]]);
  PaymentRequestNavigationController* navigation_controller =
      base::mac::ObjCCastStrict<PaymentRequestNavigationController>(
          base_view_controller.presentedViewController);
  PaymentRequestEditViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator paymentRequestEditViewController:view_controller
                         didFinishEditingFields:GetEditorFields(false)];

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that calling the view controller delegate method which signals that the
// user has finished editing a payment method, causes the corresponding
// coordinator delegate method to get called. The payment method should not get
// re-added to the PaymentRequest nor the PersonalDataManager.
TEST_F(PaymentRequestCreditCardEditCoordinatorTest, DidFinishEditing) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  CreditCardEditCoordinator* coordinator = [[CreditCardEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Set the payment method to be edited.
  autofill::CreditCard credit_card;
  payments::AutofillPaymentInstrument payment_method(
      "", credit_card, false, payment_request_->billing_profiles(), "", nil);
  [coordinator setPaymentMethod:&payment_method];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(CreditCardEditCoordinatorDelegate)];
  [[delegate expect]
          creditCardEditCoordinator:coordinator
      didFinishEditingPaymentMethod:static_cast<
                                        payments::AutofillPaymentInstrument*>(
                                        [OCMArg anyPointer])];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(nil, base_view_controller.presentedViewController);

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_NE(nil, base_view_controller.presentedViewController);

  // No payment method should get added to the PaymentRequest.
  EXPECT_CALL(*payment_request_, AddAutofillPaymentInstrument(_)).Times(0);
  // No payment method should get added to the PersonalDataManager.
  EXPECT_CALL(personal_data_manager_, AddCreditCard(_)).Times(0);
  // Expect a payment method to be updated in the PersonalDataManager.
  EXPECT_CALL(personal_data_manager_,
              UpdateCreditCard(CreditCardMatches("4111111111111111", "John Doe",
                                                 "12", "2090", "12345")))
      .Times(1);

  // Call the controller delegate method.
  EXPECT_TRUE([base_view_controller.presentedViewController
      isMemberOfClass:[PaymentRequestNavigationController class]]);
  PaymentRequestNavigationController* navigation_controller =
      base::mac::ObjCCastStrict<PaymentRequestNavigationController>(
          base_view_controller.presentedViewController);
  PaymentRequestEditViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator paymentRequestEditViewController:view_controller
                         didFinishEditingFields:GetEditorFields(true)];

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that calling the view controller delegate method which signals that the
// user has chosen to cancel creating/editing a payment method, causes the
// corresponding coordinator delegate method to get called.
TEST_F(PaymentRequestCreditCardEditCoordinatorTest, DidCancel) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  CreditCardEditCoordinator* coordinator = [[CreditCardEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(CreditCardEditCoordinatorDelegate)];
  [[delegate expect] creditCardEditCoordinatorDidCancel:coordinator];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(nil, base_view_controller.presentedViewController);

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_NE(nil, base_view_controller.presentedViewController);

  // Call the controller delegate method.
  EXPECT_TRUE([base_view_controller.presentedViewController
      isMemberOfClass:[PaymentRequestNavigationController class]]);
  PaymentRequestNavigationController* navigation_controller =
      base::mac::ObjCCastStrict<PaymentRequestNavigationController>(
          base_view_controller.presentedViewController);
  PaymentRequestEditViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator paymentRequestEditViewControllerDidCancel:view_controller];

  EXPECT_OCMOCK_VERIFY(delegate);
}
