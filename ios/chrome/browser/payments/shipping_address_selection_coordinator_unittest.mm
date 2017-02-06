// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/shipping_address_selection_coordinator.h"

#include "base/memory/ptr_util.h"
#include "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Tests that invoking start and stop on the coordinator presents and dismisses
// the ShippingAddressSelectionViewController, respectively.
TEST(ShippingAddressSelectionCoordinatorTest, StartAndStop) {
  @autoreleasepool {
    UIViewController* base_view_controller =
        [[[UIViewController alloc] init] autorelease];
    UINavigationController* navigation_controller =
        [[[UINavigationController alloc]
            initWithRootViewController:base_view_controller] autorelease];
    ShippingAddressSelectionCoordinator* coordinator =
        [[[ShippingAddressSelectionCoordinator alloc]
            initWithBaseViewController:base_view_controller] autorelease];

    autofill::TestPersonalDataManager personal_data_manager;
    std::unique_ptr<PaymentRequest> payment_request =
        base::MakeUnique<PaymentRequest>(
            base::MakeUnique<web::PaymentRequest>(), &personal_data_manager);
    [coordinator setPaymentRequest:payment_request.get()];

    EXPECT_EQ(1u, [navigation_controller.viewControllers count]);

    [coordinator start];
    // Short delay to allow animation to complete.
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::TimeDelta::FromSecondsD(1.0));
    EXPECT_EQ(2u, [navigation_controller.viewControllers count]);

    [coordinator stop];
    // Short delay to allow animation to complete.
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::TimeDelta::FromSecondsD(1.0));
    EXPECT_EQ(1u, [navigation_controller.viewControllers count]);
  }
}

}  // namespace
