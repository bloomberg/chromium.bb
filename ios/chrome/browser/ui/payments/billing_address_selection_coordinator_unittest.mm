// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/billing_address_selection_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/ui/payments/payment_request_selector_view_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PaymentRequestBillingAddressSelectionCoordinatorTest
    : public PlatformTest {
 protected:
  PaymentRequestBillingAddressSelectionCoordinatorTest()
      : autofill_profile1_(autofill::test::GetFullProfile()),
        autofill_profile2_(autofill::test::GetFullProfile2()) {
    // Add testing profiles to autofill::TestPersonalDataManager.
    personal_data_manager_.AddTestingProfile(&autofill_profile1_);
    personal_data_manager_.AddTestingProfile(&autofill_profile2_);
    payment_request_ = base::MakeUnique<PaymentRequest>(
        payment_request_test_util::CreateTestWebPaymentRequest(),
        &personal_data_manager_);
  }

  void SetUp() override {
    UIViewController* base_view_controller = [[UIViewController alloc] init];
    navigation_controller_ = [[UINavigationController alloc]
        initWithRootViewController:base_view_controller];

    coordinator_ = [[BillingAddressSelectionCoordinator alloc]
        initWithBaseViewController:base_view_controller];
    [coordinator_ setPaymentRequest:payment_request_.get()];
  }

  UINavigationController* GetNavigationController() {
    return navigation_controller_;
  }

  BillingAddressSelectionCoordinator* GetCoordinator() { return coordinator_; }

  UINavigationController* navigation_controller_;
  BillingAddressSelectionCoordinator* coordinator_;

  autofill::AutofillProfile autofill_profile1_;
  autofill::AutofillProfile autofill_profile2_;
  autofill::TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<PaymentRequest> payment_request_;
};

// Tests that invoking start and stop on the coordinator presents and dismisses
// the PaymentRequestSelectorViewController, respectively.
TEST_F(PaymentRequestBillingAddressSelectionCoordinatorTest, StartAndStop) {
  EXPECT_EQ(1u, GetNavigationController().viewControllers.count);

  [GetCoordinator() start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, GetNavigationController().viewControllers.count);

  UIViewController* view_controller =
      GetNavigationController().visibleViewController;
  EXPECT_TRUE([view_controller
      isMemberOfClass:[PaymentRequestSelectorViewController class]]);

  [GetCoordinator() stop];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(1u, GetNavigationController().viewControllers.count);
}

// Tests that calling the view controller delegate method which notifies the
// delegate about selection of a billing address invokes the corresponding
// coordinator delegate method.
TEST_F(PaymentRequestBillingAddressSelectionCoordinatorTest,
       SelectedBillingAddress) {
  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(BillingAddressSelectionCoordinatorDelegate)];
  autofill::AutofillProfile* profile = payment_request_->billing_profiles()[1];
  [[delegate expect] billingAddressSelectionCoordinator:GetCoordinator()
                                didSelectBillingAddress:profile];
  [GetCoordinator() setDelegate:delegate];

  EXPECT_EQ(1u, GetNavigationController().viewControllers.count);

  [GetCoordinator() start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, GetNavigationController().viewControllers.count);

  // Call the controller delegate method.
  PaymentRequestSelectorViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestSelectorViewController>(
          GetNavigationController().visibleViewController);
  [GetCoordinator() paymentRequestSelectorViewController:view_controller
                                    didSelectItemAtIndex:1];

  // Wait for the coordinator delegate to be notified.
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(0.5));

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that calling the view controller delegate method which notifies the
// delegate that the user has chosen to return without making a selection
// invokes the corresponding coordinator delegate method.
TEST_F(PaymentRequestBillingAddressSelectionCoordinatorTest, DidReturn) {
  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(BillingAddressSelectionCoordinatorDelegate)];
  [[delegate expect]
      billingAddressSelectionCoordinatorDidReturn:GetCoordinator()];
  [GetCoordinator() setDelegate:delegate];

  EXPECT_EQ(1u, GetNavigationController().viewControllers.count);

  [GetCoordinator() start];
  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, GetNavigationController().viewControllers.count);

  // Call the controller delegate method.
  PaymentRequestSelectorViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestSelectorViewController>(
          GetNavigationController().visibleViewController);
  [GetCoordinator()
      paymentRequestSelectorViewControllerDidFinish:view_controller];

  EXPECT_OCMOCK_VERIFY(delegate);
}
