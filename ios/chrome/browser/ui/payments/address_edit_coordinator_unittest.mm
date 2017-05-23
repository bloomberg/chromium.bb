// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/address_edit_coordinator.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/test_region_data_loader.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#include "ios/chrome/browser/payments/test_payment_request.h"
#import "ios/chrome/browser/ui/payments/address_edit_view_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PaymentRequestAddressEditCoordinatorTest : public PlatformTest {
 protected:
  PaymentRequestAddressEditCoordinatorTest()
      : pref_service_(autofill::test::PrefServiceForTesting()) {
    personal_data_manager_.SetTestingPrefService(pref_service_.get());
    payment_request_ = base::MakeUnique<TestPaymentRequest>(
        payment_request_test_util::CreateTestWebPaymentRequest(),
        &personal_data_manager_);
    test_region_data_loader_.set_synchronous_callback(true);
    payment_request_->SetRegionDataLoader(&test_region_data_loader_);
  }

  void TearDown() override {
    personal_data_manager_.SetTestingPrefService(nullptr);
  }

  autofill::TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<TestPaymentRequest> payment_request_;
  autofill::TestRegionDataLoader test_region_data_loader_;
};

// Tests that invoking start and stop on the coordinator presents and dismisses
// the address edit view controller, respectively.
TEST_F(PaymentRequestAddressEditCoordinatorTest, StartAndStop) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  AddressEditCoordinator* coordinator = [[AddressEditCoordinator alloc]
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
// user has finished creating/editing an address, causes the corresponding
// coordinator delegate method to get called.
TEST_F(PaymentRequestAddressEditCoordinatorTest, DidFinishEditing) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  AddressEditCoordinator* coordinator = [[AddressEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate =
      [OCMockObject mockForProtocol:@protocol(AddressEditCoordinatorDelegate)];
  [[delegate expect]
       addressEditCoordinator:coordinator
      didFinishEditingAddress:static_cast<autofill::AutofillProfile*>(
                                  [OCMArg anyPointer])];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(1u, navigation_controller.viewControllers.count);

  [coordinator start];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, navigation_controller.viewControllers.count);

  // Call the controller delegate method.
  AddressEditViewController* view_controller =
      base::mac::ObjCCastStrict<AddressEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator addressEditViewController:view_controller
                  didFinishEditingFields:@[]];

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that calling the view controller delegate method which signals that the
// user has chosen to cancel creating/editing an address, causes the
// corresponding coordinator delegate method to get called.
TEST_F(PaymentRequestAddressEditCoordinatorTest, DidCancel) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  AddressEditCoordinator* coordinator = [[AddressEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Mock the coordinator delegate.
  id delegate =
      [OCMockObject mockForProtocol:@protocol(AddressEditCoordinatorDelegate)];
  [[delegate expect] addressEditCoordinatorDidCancel:coordinator];
  [coordinator setDelegate:delegate];

  EXPECT_EQ(1u, navigation_controller.viewControllers.count);

  [coordinator start];
  // Short delay to allow animation to complete.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_EQ(2u, navigation_controller.viewControllers.count);

  // Call the controller delegate method.
  AddressEditViewController* view_controller =
      base::mac::ObjCCastStrict<AddressEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator addressEditViewControllerDidCancel:view_controller];

  EXPECT_OCMOCK_VERIFY(delegate);
}
