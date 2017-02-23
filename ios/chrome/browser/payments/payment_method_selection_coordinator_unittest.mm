// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_method_selection_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#import "ios/chrome/browser/payments/payment_method_selection_view_controller.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef PlatformTest PaymentMethodSelectionCoordinatorTest;

// Tests that invoking start and stop on the coordinator presents and dismisses
// the PaymentMethodSelectionViewController, respectively.
TEST(PaymentMethodSelectionCoordinatorTest, StartAndStop) {
  std::unique_ptr<PaymentRequest> payment_request =
      payment_request_test_util::CreateTestPaymentRequest();

  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  PaymentMethodSelectionCoordinator* coordinator =
      [[PaymentMethodSelectionCoordinator alloc]
          initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request.get()];

  EXPECT_EQ(1u, navigation_controller.viewControllers.count);

  [coordinator start];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, navigation_controller.viewControllers.count);

  UIViewController* view_controller =
      navigation_controller.visibleViewController;
  EXPECT_TRUE([view_controller
      isMemberOfClass:[PaymentMethodSelectionViewController class]]);

  [coordinator stop];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(1u, navigation_controller.viewControllers.count);
}

// Tests that calling the view controller delegate method which notifies the
// coordinator about selection of a payment method invokes the corresponding
// coordinator delegate method.
TEST(PaymentMethodSelectionCoordinatorTest, DidSelectPaymentMethod) {
  std::unique_ptr<PaymentRequest> payment_request =
      payment_request_test_util::CreateTestPaymentRequest();

  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  PaymentMethodSelectionCoordinator* coordinator =
      [[PaymentMethodSelectionCoordinator alloc]
          initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  std::unique_ptr<autofill::CreditCard> credit_card(new autofill::CreditCard());
  [[delegate expect] paymentMethodSelectionCoordinator:coordinator
                                didSelectPaymentMethod:credit_card.get()];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(1u, navigation_controller.viewControllers.count);

  [coordinator start];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, navigation_controller.viewControllers.count);

  // Call the controller delegate method.
  PaymentMethodSelectionViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentMethodSelectionViewController>(
          navigation_controller.visibleViewController);
  [coordinator paymentMethodSelectionViewController:view_controller
                             didSelectPaymentMethod:credit_card.get()];

  // Wait for the coordinator delegate to be notified.
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(0.5));

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that calling the view controller delegate method which notifies the
// coordinator that the user has chosen to return without making a selection
// invokes the corresponding coordinator delegate method.
TEST(PaymentMethodSelectionCoordinatorTest, DidReturn) {
  std::unique_ptr<PaymentRequest> payment_request =
      payment_request_test_util::CreateTestPaymentRequest();

  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  PaymentMethodSelectionCoordinator* coordinator =
      [[PaymentMethodSelectionCoordinator alloc]
          initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentMethodSelectionCoordinatorDelegate)];
  [[delegate expect] paymentMethodSelectionCoordinatorDidReturn:coordinator];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(1u, navigation_controller.viewControllers.count);

  [coordinator start];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, navigation_controller.viewControllers.count);

  // Call the controller delegate method.
  PaymentMethodSelectionViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentMethodSelectionViewController>(
          navigation_controller.visibleViewController);
  [coordinator paymentMethodSelectionViewControllerDidReturn:view_controller];

  EXPECT_OCMOCK_VERIFY(delegate);
}
