// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_method_selection_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/test/ios/wait_util.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/core/payment_instrument.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#include "ios/chrome/browser/payments/test_payment_request.h"
#import "ios/chrome/browser/ui/payments/payment_request_selector_view_controller.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PaymentRequestPaymentMethodSelectionCoordinatorTest
    : public PlatformTest {
 protected:
  PaymentRequestPaymentMethodSelectionCoordinatorTest()
      : autofill_profile_(autofill::test::GetFullProfile()),
        credit_card1_(autofill::test::GetCreditCard()),
        credit_card2_(autofill::test::GetCreditCard2()) {
    // Add testing credit cards to autofill::TestPersonalDataManager. Make the
    // less frequently used one incomplete.
    credit_card1_.set_use_count(10U);
    personal_data_manager_.AddTestingProfile(&autofill_profile_);
    credit_card1_.set_billing_address_id(autofill_profile_.guid());
    personal_data_manager_.AddTestingCreditCard(&credit_card1_);
    credit_card2_.set_use_count(5U);
    personal_data_manager_.AddTestingCreditCard(&credit_card2_);
    payment_request_ = base::MakeUnique<TestPaymentRequest>(
        payment_request_test_util::CreateTestWebPaymentRequest(),
        &personal_data_manager_);
  }

  base::test::ScopedTaskEnvironment scoped_task_evironment_;

  autofill::AutofillProfile autofill_profile_;
  autofill::CreditCard credit_card1_;
  autofill::CreditCard credit_card2_;
  autofill::TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<TestPaymentRequest> payment_request_;
};

// Tests that invoking start and stop on the coordinator presents and dismisses
// the PaymentRequestSelectorViewController, respectively.
TEST_F(PaymentRequestPaymentMethodSelectionCoordinatorTest, StartAndStop) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  PaymentMethodSelectionCoordinator* coordinator =
      [[PaymentMethodSelectionCoordinator alloc]
          initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  EXPECT_EQ(1u, navigation_controller.viewControllers.count);

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, navigation_controller.viewControllers.count);

  UIViewController* view_controller =
      navigation_controller.visibleViewController;
  EXPECT_TRUE([view_controller
      isMemberOfClass:[PaymentRequestSelectorViewController class]]);

  [coordinator stop];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(1u, navigation_controller.viewControllers.count);
}

// Tests that calling the view controller delegate method which notifies the
// coordinator about selection of a payment method invokes the corresponding
// coordinator delegate method, only if the payment method is complete.
TEST_F(PaymentRequestPaymentMethodSelectionCoordinatorTest,
       DidSelectPaymentMethod) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  PaymentMethodSelectionCoordinator* coordinator =
      [[PaymentMethodSelectionCoordinator alloc]
          initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  [[delegate expect]
      paymentMethodSelectionCoordinator:coordinator
                 didSelectPaymentMethod:payment_request_->payment_methods()[0]];
  [[delegate reject]
      paymentMethodSelectionCoordinator:coordinator
                 didSelectPaymentMethod:payment_request_->payment_methods()[1]];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(1u, navigation_controller.viewControllers.count);

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, navigation_controller.viewControllers.count);

  // Call the controller delegate method for both selectable items.
  PaymentRequestSelectorViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestSelectorViewController>(
          navigation_controller.visibleViewController);
  EXPECT_TRUE([coordinator paymentRequestSelectorViewController:view_controller
                                           didSelectItemAtIndex:0]);
  // Wait for the coordinator delegate to be notified.
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(0.5));
  EXPECT_FALSE([coordinator paymentRequestSelectorViewController:view_controller
                                            didSelectItemAtIndex:1]);

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that calling the view controller delegate method which notifies the
// coordinator that the user has chosen to return without making a selection
// invokes the corresponding coordinator delegate method.
TEST_F(PaymentRequestPaymentMethodSelectionCoordinatorTest, DidReturn) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  PaymentMethodSelectionCoordinator* coordinator =
      [[PaymentMethodSelectionCoordinator alloc]
          initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  [[delegate expect] paymentMethodSelectionCoordinatorDidReturn:coordinator];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(1u, navigation_controller.viewControllers.count);

  [coordinator start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, navigation_controller.viewControllers.count);

  // Call the controller delegate method.
  PaymentRequestSelectorViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestSelectorViewController>(
          navigation_controller.visibleViewController);
  [coordinator paymentRequestSelectorViewControllerDidFinish:view_controller];

  EXPECT_OCMOCK_VERIFY(delegate);
}
