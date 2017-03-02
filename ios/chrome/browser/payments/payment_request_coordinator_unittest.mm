// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_request_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/payments/payment_request_view_controller.h"
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

typedef PlatformTest PaymentRequestCoordinatorTest;

@interface PaymentRequestCoordinatorDelegateMock<
    PaymentRequestCoordinatorDelegate>:OCMockComplexTypeHelper
@end

@implementation PaymentRequestCoordinatorDelegateMock

typedef void (^mock_coordinator_cancel)(PaymentRequestCoordinator*);
typedef void (^mock_coordinator_confirm)(PaymentRequestCoordinator*,
                                         web::PaymentResponse);
typedef void (^mock_coordinator_select_shipping_address)(
    PaymentRequestCoordinator*,
    web::PaymentAddress);
typedef void (^mock_coordinator_select_shipping_option)(
    PaymentRequestCoordinator*,
    web::PaymentShippingOption);

- (void)paymentRequestCoordinatorDidCancel:
    (PaymentRequestCoordinator*)coordinator {
  return static_cast<mock_coordinator_cancel>([self blockForSelector:_cmd])(
      coordinator);
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
    didConfirmWithPaymentResponse:(web::PaymentResponse)paymentResponse {
  return static_cast<mock_coordinator_confirm>([self blockForSelector:_cmd])(
      coordinator, paymentResponse);
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
         didSelectShippingAddress:(web::PaymentAddress)shippingAddress {
  return static_cast<mock_coordinator_select_shipping_address>(
      [self blockForSelector:_cmd])(coordinator, shippingAddress);
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
          didSelectShippingOption:(web::PaymentShippingOption)shippingOption {
  return static_cast<mock_coordinator_select_shipping_option>(
      [self blockForSelector:_cmd])(coordinator, shippingOption);
}

@end

// Tests that invoking start and stop on the coordinator presents and
// dismisses
// the PaymentRequestViewController, respectively.
TEST(PaymentRequestCoordinatorTest, StartAndStop) {
  std::unique_ptr<PaymentRequest> payment_request =
      payment_request_test_util::CreateTestPaymentRequest();

  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request.get()];

  [coordinator start];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1));
  id presented_view_controller =
      [coordinator baseViewController].presentedViewController;
  EXPECT_TRUE([presented_view_controller
      isMemberOfClass:[UINavigationController class]]);
  UINavigationController* navigation_controller =
      base::mac::ObjCCastStrict<UINavigationController>(
          presented_view_controller);
  EXPECT_EQ(1u, navigation_controller.viewControllers.count);
  id view_controller = navigation_controller.visibleViewController;
  EXPECT_TRUE(
      [view_controller isMemberOfClass:[PaymentRequestViewController class]]);

  [coordinator stop];
  // Delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(2));

  EXPECT_EQ(nil, [coordinator baseViewController].presentedViewController);
}

// Tests that calling the card unmasking delegate method which notifies the
// coordinator about successful unmasking of a credit card invokes the
// appropriate coordinator delegate method with the expected information.
TEST(PaymentRequestCoordinatorTest, FullCardRequestDidSucceed) {
  std::unique_ptr<PaymentRequest> payment_request =
      payment_request_test_util::CreateTestPaymentRequest();

  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request.get()];

  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  id delegate_mock([[PaymentRequestCoordinatorDelegateMock alloc]
      initWithRepresentedObject:delegate]);
  SEL selector =
      @selector(paymentRequestCoordinator:didConfirmWithPaymentResponse:);
  [delegate_mock onSelector:selector
       callBlockExpectation:^(PaymentRequestCoordinator* callerCoordinator,
                              web::PaymentResponse paymentResponse) {
         EXPECT_EQ(base::ASCIIToUTF16("411111111111"),
                   paymentResponse.details.card_number);
         EXPECT_EQ(base::ASCIIToUTF16("John Doe"),
                   paymentResponse.details.cardholder_name);
         EXPECT_EQ(base::ASCIIToUTF16("01"),
                   paymentResponse.details.expiry_month);
         EXPECT_EQ(base::ASCIIToUTF16("2999"),
                   paymentResponse.details.expiry_year);
         EXPECT_EQ(base::ASCIIToUTF16("123"),
                   paymentResponse.details.card_security_code);
         EXPECT_EQ(coordinator, callerCoordinator);
       }];
  [coordinator setDelegate:delegate_mock];

  // Call the card unmasking delegate method.
  std::unique_ptr<autofill::CreditCard> card =
      payment_request_test_util::CreateTestCreditCard();
  [coordinator fullCardRequestDidSucceedWithCard:*card
                                             CVC:base::ASCIIToUTF16("123")];
}

// Tests that calling the ShippingAddressSelectionCoordinator delegate method
// which notifies the coordinator about selection of a shipping address invokes
// the corresponding coordinator delegate method with the expected information.
TEST(PaymentRequestCoordinatorTest, DidSelectShippingAddress) {
  std::unique_ptr<PaymentRequest> payment_request =
      payment_request_test_util::CreateTestPaymentRequest();

  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  id delegate_mock([[PaymentRequestCoordinatorDelegateMock alloc]
      initWithRepresentedObject:delegate]);
  SEL selector = @selector(paymentRequestCoordinator:didSelectShippingAddress:);
  [delegate_mock onSelector:selector
       callBlockExpectation:^(PaymentRequestCoordinator* callerCoordinator,
                              web::PaymentAddress shippingAddress) {
         EXPECT_EQ(base::ASCIIToUTF16("John Mitchell Doe"),
                   shippingAddress.recipient);
         EXPECT_EQ(base::ASCIIToUTF16("Fox"), shippingAddress.organization);
         ASSERT_EQ(2U, shippingAddress.address_line.size());
         EXPECT_EQ(base::ASCIIToUTF16("123 Zoo St"),
                   shippingAddress.address_line[0]);
         EXPECT_EQ(base::ASCIIToUTF16("unit 5"),
                   shippingAddress.address_line[1]);
         EXPECT_EQ(base::ASCIIToUTF16("12345678910"), shippingAddress.phone);
         EXPECT_EQ(base::ASCIIToUTF16("Hollywood"), shippingAddress.city);
         EXPECT_EQ(base::ASCIIToUTF16("CA"), shippingAddress.region);
         EXPECT_EQ(base::ASCIIToUTF16("US"), shippingAddress.country);
         EXPECT_EQ(base::ASCIIToUTF16("91601"), shippingAddress.postal_code);
         EXPECT_EQ(coordinator, callerCoordinator);
       }];
  [coordinator setDelegate:delegate_mock];

  // Call the ShippingAddressSelectionCoordinator delegate method.
  std::unique_ptr<autofill::AutofillProfile> profile =
      payment_request_test_util::CreateTestAutofillProfile();
  [coordinator shippingAddressSelectionCoordinator:nil
                          didSelectShippingAddress:profile.get()];
}

// Tests that calling the ShippingOptionSelectionCoordinator delegate method
// which notifies the coordinator about selection of a shipping option invokes
// the corresponding coordinator delegate method with the expected information.
TEST(PaymentRequestCoordinatorTest, DidSelectShippingOption) {
  std::unique_ptr<PaymentRequest> payment_request =
      payment_request_test_util::CreateTestPaymentRequest();

  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  id delegate_mock([[PaymentRequestCoordinatorDelegateMock alloc]
      initWithRepresentedObject:delegate]);
  SEL selector = @selector(paymentRequestCoordinator:didSelectShippingOption:);
  [delegate_mock onSelector:selector
       callBlockExpectation:^(PaymentRequestCoordinator* callerCoordinator,
                              web::PaymentShippingOption shippingOption) {
         EXPECT_EQ(base::ASCIIToUTF16("123456"), shippingOption.id);
         EXPECT_EQ(base::ASCIIToUTF16("1-Day"), shippingOption.label);
         EXPECT_EQ(base::ASCIIToUTF16("0.99"), shippingOption.amount.value);
         EXPECT_EQ(base::ASCIIToUTF16("USD"), shippingOption.amount.currency);
         EXPECT_EQ(coordinator, callerCoordinator);
       }];
  [coordinator setDelegate:delegate_mock];

  // Call the ShippingOptionSelectionCoordinator delegate method.
  web::PaymentShippingOption shipping_option;
  shipping_option.id = base::ASCIIToUTF16("123456");
  shipping_option.label = base::ASCIIToUTF16("1-Day");
  shipping_option.amount.value = base::ASCIIToUTF16("0.99");
  shipping_option.amount.currency = base::ASCIIToUTF16("USD");
  [coordinator shippingOptionSelectionCoordinator:nil
                          didSelectShippingOption:&shipping_option];
}

// Tests that calling the view controller delegate method which notifies the
// coordinator about cancellation of the PaymentRequest invokes the
// corresponding coordinator delegate method.
TEST(PaymentRequestCoordinatorTest, DidCancel) {
  std::unique_ptr<PaymentRequest> payment_request =
      payment_request_test_util::CreateTestPaymentRequest();

  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestCoordinator* coordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request.get()];

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

  [coordinator start];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1));

  // Call the controller delegate method.
  [coordinator paymentRequestViewControllerDidCancel:nil];
}
