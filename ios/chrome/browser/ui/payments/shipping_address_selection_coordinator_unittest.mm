// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/shipping_address_selection_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/test/ios/wait_util.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/test_region_data_loader.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#include "ios/chrome/browser/payments/test_payment_request.h"
#import "ios/chrome/browser/ui/payments/payment_request_selector_view_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PaymentRequestShippingAddressSelectionCoordinatorTest
    : public PlatformTest {
 protected:
  PaymentRequestShippingAddressSelectionCoordinatorTest()
      : autofill_profile1_(autofill::test::GetFullProfile()),
        autofill_profile2_(autofill::test::GetFullProfile2()),
        pref_service_(autofill::test::PrefServiceForTesting()) {
    personal_data_manager_.SetTestingPrefService(pref_service_.get());
    // Add testing profiles to autofill::TestPersonalDataManager. Make the less
    // frequently used one incomplete.
    autofill_profile1_.set_use_count(10U);
    personal_data_manager_.AddTestingProfile(&autofill_profile1_);
    autofill_profile2_.set_use_count(5U);
    autofill_profile2_.SetInfo(
        autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
        base::string16(), "en-US");
    personal_data_manager_.AddTestingProfile(&autofill_profile2_);
    payment_request_ = base::MakeUnique<TestPaymentRequest>(
        payment_request_test_util::CreateTestWebPaymentRequest(),
        &personal_data_manager_);

    test_region_data_loader_.set_synchronous_callback(true);
    payment_request_->SetRegionDataLoader(&test_region_data_loader_);
  }

  void TearDown() override {
    personal_data_manager_.SetTestingPrefService(nullptr);
  }

  base::test::ScopedTaskEnvironment scoped_task_evironment_;

  autofill::AutofillProfile autofill_profile1_;
  autofill::AutofillProfile autofill_profile2_;
  std::unique_ptr<PrefService> pref_service_;
  autofill::TestPersonalDataManager personal_data_manager_;
  autofill::TestRegionDataLoader test_region_data_loader_;
  std::unique_ptr<TestPaymentRequest> payment_request_;
};

// Tests that invoking start and stop on the coordinator presents and dismisses
// the PaymentRequestSelectorViewController, respectively.
TEST_F(PaymentRequestShippingAddressSelectionCoordinatorTest, StartAndStop) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  ShippingAddressSelectionCoordinator* coordinator =
      [[ShippingAddressSelectionCoordinator alloc]
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
// delegate about selection of a shipping address invokes the corresponding
// coordinator delegate method, only if the payment method is complete.
TEST_F(PaymentRequestShippingAddressSelectionCoordinatorTest,
       SelectedShippingAddress) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  ShippingAddressSelectionCoordinator* coordinator =
      [[ShippingAddressSelectionCoordinator alloc]
          initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(ShippingAddressSelectionCoordinatorDelegate)];
  [[delegate expect]
      shippingAddressSelectionCoordinator:coordinator
                 didSelectShippingAddress:payment_request_
                                              ->shipping_profiles()[0]];
  [[delegate reject]
      shippingAddressSelectionCoordinator:coordinator
                 didSelectShippingAddress:payment_request_
                                              ->shipping_profiles()[1]];
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
  [coordinator paymentRequestSelectorViewController:view_controller
                               didSelectItemAtIndex:0];
  // Wait for the coordinator delegate to be notified.
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(0.5));
  [coordinator paymentRequestSelectorViewController:view_controller
                               didSelectItemAtIndex:1];
  // Wait for the coordinator delegate to be notified.
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(0.5));

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that calling the view controller delegate method which notifies the
// delegate that the user has chosen to return without making a selection
// invokes the corresponding coordinator delegate method.
TEST_F(PaymentRequestShippingAddressSelectionCoordinatorTest, DidReturn) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  ShippingAddressSelectionCoordinator* coordinator =
      [[ShippingAddressSelectionCoordinator alloc]
          initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate = [OCMockObject
      mockForProtocol:@protocol(ShippingAddressSelectionCoordinatorDelegate)];
  [[delegate expect] shippingAddressSelectionCoordinatorDidReturn:coordinator];
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
