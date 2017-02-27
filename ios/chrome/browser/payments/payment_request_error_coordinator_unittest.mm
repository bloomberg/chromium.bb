// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_request_error_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/test/ios/wait_util.h"
#import "ios/chrome/browser/payments/payment_request_error_view_controller.h"
#import "ios/chrome/test/scoped_key_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

typedef PlatformTest PaymentRequestErrorCoordinatorTest;

// Tests that invoking start and stop on the coordinator presents and dismisses
// the payment request error view controller, respectively.
TEST(PaymentRequestErrorCoordinatorTest, StartAndStop) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestErrorCoordinator* coordinator =
      [[PaymentRequestErrorCoordinator alloc]
          initWithBaseViewController:base_view_controller];

  [coordinator start];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  id presented_view_controller =
      [coordinator baseViewController].presentedViewController;
  EXPECT_TRUE([presented_view_controller
      isMemberOfClass:[PaymentRequestErrorViewController class]]);

  [coordinator stop];
  // Delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(2));
  EXPECT_EQ(nil, [coordinator baseViewController].presentedViewController);
}

// Tests that calling the view controller delegate method which notifies the
// coordinator that the user has dismissed the error invokes the corresponding
// coordinator delegate method.
TEST(PaymentRequestErrorCoordinatorTest, DidDismiss) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  PaymentRequestErrorCoordinator* coordinator =
      [[PaymentRequestErrorCoordinator alloc]
          initWithBaseViewController:base_view_controller];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(PaymentRequestErrorCoordinatorDelegate)];
  [[delegate expect] paymentRequestErrorCoordinatorDidDismiss:coordinator];
  [coordinator setDelegate:delegate];

  [coordinator start];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));

  // Call the controller delegate method.
  id presented_view_controller =
      [coordinator baseViewController].presentedViewController;
  PaymentRequestErrorViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestErrorViewController>(
          presented_view_controller);
  [coordinator paymentRequestErrorViewControllerDidDismiss:view_controller];

  EXPECT_OCMOCK_VERIFY(delegate);
}
