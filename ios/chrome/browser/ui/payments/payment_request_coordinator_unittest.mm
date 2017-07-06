// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_coordinator.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/core/autofill_payment_instrument.h"
#include "components/payments/core/payment_address.h"
#include "components/payments/core/payment_instrument.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payments_test_util.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#include "ios/chrome/browser/payments/test_payment_request.h"
#include "ios/chrome/browser/signin/fake_signin_manager_builder.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#import "ios/chrome/browser/ui/payments/payment_request_view_controller.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#include "ios/web/public/payments/payment_request.h"
#import "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PaymentRequestCoordinatorDelegateMock<
    PaymentRequestCoordinatorDelegate>:OCMockComplexTypeHelper
@end

@implementation PaymentRequestCoordinatorDelegateMock

typedef void (^mock_coordinator_cancel)(PaymentRequestCoordinator*);
typedef void (^mock_coordinator_complete)(PaymentRequestCoordinator*,
                                          const std::string&,
                                          const std::string&);
typedef void (^mock_coordinator_select_shipping_address)(
    PaymentRequestCoordinator*,
    const autofill::AutofillProfile&);
typedef void (^mock_coordinator_select_shipping_option)(
    PaymentRequestCoordinator*,
    const web::PaymentShippingOption&);

- (void)paymentRequestCoordinatorDidCancel:
    (PaymentRequestCoordinator*)coordinator {
  return static_cast<mock_coordinator_cancel>([self blockForSelector:_cmd])(
      coordinator);
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
         didReceiveFullMethodName:(const std::string&)methodName
               stringifiedDetails:(const std::string&)stringifiedDetails {
  return static_cast<mock_coordinator_complete>([self blockForSelector:_cmd])(
      coordinator, methodName, stringifiedDetails);
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
         didSelectShippingAddress:
             (const autofill::AutofillProfile&)shippingAddress {
  return static_cast<mock_coordinator_select_shipping_address>(
      [self blockForSelector:_cmd])(coordinator, shippingAddress);
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
          didSelectShippingOption:
              (const web::PaymentShippingOption&)shippingOption {
  return static_cast<mock_coordinator_select_shipping_option>(
      [self blockForSelector:_cmd])(coordinator, shippingOption);
}

@end

class PaymentRequestCoordinatorTest : public PlatformTest {
 protected:
  PaymentRequestCoordinatorTest()
      : autofill_profile_(autofill::test::GetFullProfile()),
        credit_card_(autofill::test::GetCreditCard()),
        pref_service_(payments::test::PrefServiceForTesting()) {
    // Add testing profile and credit card to autofill::TestPersonalDataManager.
    personal_data_manager_.AddTestingProfile(&autofill_profile_);
    personal_data_manager_.AddTestingCreditCard(&credit_card_);

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(ios::SigninManagerFactory::GetInstance(),
                                       &ios::BuildFakeSigninManager);
    browser_state_ = test_cbs_builder.Build();

    payment_request_ = base::MakeUnique<payments::TestPaymentRequest>(
        payment_request_test_util::CreateTestWebPaymentRequest(),
        browser_state_.get(), &personal_data_manager_);
    payment_request_->SetPrefService(pref_service_.get());
  }

  base::test::ScopedTaskEnvironment scoped_task_evironment_;

  autofill::AutofillProfile autofill_profile_;
  autofill::CreditCard credit_card_;
  std::unique_ptr<PrefService> pref_service_;
  autofill::TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<payments::TestPaymentRequest> payment_request_;
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
};

// Tests that invoking start and stop on the coordinator presents and
// dismisses
// the PaymentRequestViewController, respectively.
TEST_F(PaymentRequestCoordinatorTest, StartAndStop) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];
  [coordinator setBrowserState:browser_state_.get()];

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1));
  EXPECT_TRUE([base_view_controller.presentedViewController
      isMemberOfClass:[UINavigationController class]]);
  UINavigationController* navigation_controller =
      base::mac::ObjCCastStrict<UINavigationController>(
          base_view_controller.presentedViewController);
  EXPECT_EQ(1u, navigation_controller.viewControllers.count);
  EXPECT_TRUE([navigation_controller.visibleViewController
      isMemberOfClass:[PaymentRequestViewController class]]);

  [coordinator stop];
  // Wait until the animation completes and the presented view controller is
  // dismissed.
  base::test::ios::WaitUntilCondition(^bool() {
    return !base_view_controller.presentedViewController;
  });
  EXPECT_EQ(nil, base_view_controller.presentedViewController);
}

// Tests that calling the FullCardRequesterConsumer delegate method which
// notifies the coordinator about successful unmasking of a credit card invokes
// the appropriate coordinator delegate method with the expected information.
TEST_F(PaymentRequestCoordinatorTest, FullCardRequestDidSucceedWithMethodName) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  id delegate_mock([[PaymentRequestCoordinatorDelegateMock alloc]
      initWithRepresentedObject:delegate]);
  SEL selector = @selector
      (paymentRequestCoordinator:didReceiveFullMethodName:stringifiedDetails:);
  [delegate_mock onSelector:selector
       callBlockExpectation:^(PaymentRequestCoordinator* callerCoordinator,
                              const std::string& methodName,
                              const std::string& stringifiedDetails) {
         EXPECT_EQ("visa", methodName);

         std::string cvc;
         std::unique_ptr<base::DictionaryValue> detailsDict =
             base::DictionaryValue::From(
                 base::JSONReader::Read(stringifiedDetails));
         detailsDict->GetString("cardSecurityCode", &cvc);
         EXPECT_EQ("123", cvc);

         EXPECT_EQ(coordinator, callerCoordinator);
       }];
  [coordinator setDelegate:delegate_mock];

  std::string methodName = "visa";
  std::string appLocale = "";

  std::unique_ptr<base::DictionaryValue> response_value =
      payments::data_util::GetBasicCardResponseFromAutofillCreditCard(
          credit_card_, base::ASCIIToUTF16("123"), autofill_profile_, appLocale)
          .ToDictionaryValue();
  std::string stringifiedDetails;
  base::JSONWriter::Write(*response_value, &stringifiedDetails);

  // Call the card unmasking delegate method.
  [coordinator fullCardRequestDidSucceedWithMethodName:methodName
                                    stringifiedDetails:stringifiedDetails];
}

// Tests that calling the ShippingAddressSelectionCoordinator delegate method
// which notifies the coordinator about selection of a shipping address invokes
// the corresponding coordinator delegate method with the expected information.
TEST_F(PaymentRequestCoordinatorTest, DidSelectShippingAddress) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  id delegate_mock([[PaymentRequestCoordinatorDelegateMock alloc]
      initWithRepresentedObject:delegate]);
  SEL selector = @selector(paymentRequestCoordinator:didSelectShippingAddress:);
  [delegate_mock
                onSelector:selector
      callBlockExpectation:^(PaymentRequestCoordinator* callerCoordinator,
                             const autofill::AutofillProfile& shippingAddress) {
        EXPECT_EQ(autofill_profile_, shippingAddress);
        EXPECT_EQ(coordinator, callerCoordinator);
      }];
  [coordinator setDelegate:delegate_mock];

  // Call the ShippingAddressSelectionCoordinator delegate method.
  [coordinator shippingAddressSelectionCoordinator:nil
                          didSelectShippingAddress:&autofill_profile_];
}

// Tests that calling the ShippingOptionSelectionCoordinator delegate method
// which notifies the coordinator about selection of a shipping option invokes
// the corresponding coordinator delegate method with the expected information.
TEST_F(PaymentRequestCoordinatorTest, DidSelectShippingOption) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  web::PaymentShippingOption shipping_option;
  shipping_option.id = base::ASCIIToUTF16("123456");
  shipping_option.label = base::ASCIIToUTF16("1-Day");
  shipping_option.amount.value = base::ASCIIToUTF16("0.99");
  shipping_option.amount.currency = base::ASCIIToUTF16("USD");

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  id delegate_mock([[PaymentRequestCoordinatorDelegateMock alloc]
      initWithRepresentedObject:delegate]);
  SEL selector = @selector(paymentRequestCoordinator:didSelectShippingOption:);
  [delegate_mock
                onSelector:selector
      callBlockExpectation:^(PaymentRequestCoordinator* callerCoordinator,
                             const web::PaymentShippingOption& shippingOption) {
        EXPECT_EQ(shipping_option, shippingOption);
        EXPECT_EQ(coordinator, callerCoordinator);
      }];
  [coordinator setDelegate:delegate_mock];

  // Call the ShippingOptionSelectionCoordinator delegate method.

  [coordinator shippingOptionSelectionCoordinator:nil
                          didSelectShippingOption:&shipping_option];
}

// Tests that calling the view controller delegate method which notifies the
// coordinator about cancellation of the PaymentRequest invokes the
// corresponding coordinator delegate method.
TEST_F(PaymentRequestCoordinatorTest, DidCancel) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  id delegate_mock([[PaymentRequestCoordinatorDelegateMock alloc]
      initWithRepresentedObject:delegate]);
  SEL selector = @selector(paymentRequestCoordinatorDidCancel:);
  [delegate_mock onSelector:selector
       callBlockExpectation:^(PaymentRequestCoordinator* callerCoordinator) {
         EXPECT_EQ(coordinator, callerCoordinator);
       }];
  [coordinator setDelegate:delegate_mock];
  [coordinator setBrowserState:browser_state_.get()];

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1));

  // Call the controller delegate method.
  [coordinator paymentRequestViewControllerDidCancel:nil];
}
