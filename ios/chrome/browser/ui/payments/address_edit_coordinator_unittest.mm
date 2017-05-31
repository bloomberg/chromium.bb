// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/address_edit_coordinator.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/test_region_data_loader.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#include "ios/chrome/browser/payments/test_payment_request.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
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
  MOCK_METHOD1(AddProfile, void(const autofill::AutofillProfile&));
  MOCK_METHOD1(UpdateProfile, void(const autofill::AutofillProfile&));
};

class MockTestPaymentRequest : public TestPaymentRequest {
 public:
  MockTestPaymentRequest(web::PaymentRequest web_payment_request,
                         autofill::PersonalDataManager* personal_data_manager)
      : TestPaymentRequest(web_payment_request, personal_data_manager) {}
  MOCK_METHOD1(AddAutofillProfile,
               autofill::AutofillProfile*(const autofill::AutofillProfile&));
};

MATCHER_P4(ProfileMatches, name, country, state, phone_number, "") {
  return arg.GetRawInfo(autofill::NAME_FULL) == base::ASCIIToUTF16(name) &&
         arg.GetRawInfo(autofill::ADDRESS_HOME_COUNTRY) ==
             base::ASCIIToUTF16(country) &&
         arg.GetRawInfo(autofill::ADDRESS_HOME_STATE) ==
             base::ASCIIToUTF16(state) &&
         arg.GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER) ==
             base::ASCIIToUTF16(phone_number);
}

NSArray<EditorField*>* GetEditorFields() {
  return @[
    [[EditorField alloc] initWithAutofillUIType:AutofillUITypeProfileFullName
                                      fieldType:EditorFieldTypeTextField
                                          label:@"Name"
                                          value:@"John Doe"
                                       required:YES],
    [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeProfileHomeAddressCountry
                     fieldType:EditorFieldTypeTextField
                         label:@"Country"
                         value:@"CA" /* Canada */
                      required:YES],
    [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeProfileHomeAddressState
                     fieldType:EditorFieldTypeTextField
                         label:@"Province"
                         value:@"Quebec"
                      required:YES],
    [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeProfileHomePhoneWholeNumber
                     fieldType:EditorFieldTypeSelector
                         label:@"Phone"
                         value:@"16502111111"
                      required:YES],
  ];
}

using ::testing::_;
}  // namespace

class PaymentRequestAddressEditCoordinatorTest : public PlatformTest {
 protected:
  PaymentRequestAddressEditCoordinatorTest()
      : pref_service_(autofill::test::PrefServiceForTesting()) {
    personal_data_manager_.SetTestingPrefService(pref_service_.get());
    payment_request_ = base::MakeUnique<MockTestPaymentRequest>(
        payment_request_test_util::CreateTestWebPaymentRequest(),
        &personal_data_manager_);
    test_region_data_loader_.set_synchronous_callback(true);
    payment_request_->SetRegionDataLoader(&test_region_data_loader_);
  }

  void TearDown() override {
    personal_data_manager_.SetTestingPrefService(nullptr);
  }

  MockTestPersonalDataManager personal_data_manager_;
  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<MockTestPaymentRequest> payment_request_;
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
// user has finished creating a new address, causes the address to be added to
// the PaymentRequest instance and the corresponding coordinator delegate method
// to get called. The new address should also get added to the
// PersonalDataManager.
TEST_F(PaymentRequestAddressEditCoordinatorTest, DidFinishCreating) {
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

  // Expect an autofill profile to be added to the PaymentRequest.
  EXPECT_CALL(*payment_request_,
              AddAutofillProfile(ProfileMatches("John Doe", "CA" /* Canada */,
                                                "Quebec", "16502111111")))
      .Times(1);
  // Expect an autofill profile to be added to the PersonalDataManager.
  EXPECT_CALL(personal_data_manager_,
              AddProfile(ProfileMatches("John Doe", "CA" /* Canada */, "Quebec",
                                        "16502111111")))
      .Times(1);
  // No autofill profile should get updated in the PersonalDataManager.
  EXPECT_CALL(personal_data_manager_, UpdateProfile(_)).Times(0);

  // Call the controller delegate method.
  PaymentRequestEditViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator paymentRequestEditViewController:view_controller
                         didFinishEditingFields:GetEditorFields()];

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that calling the view controller delegate method which signals that the
// user has finished editing an address, causes the the corresponding
// coordinator delegate method to get called. The address should not get
// re-added to the PaymentRequest instance or the PersonalDataManager. However,
// it is expected to get updated in the PersonalDataManager.
TEST_F(PaymentRequestAddressEditCoordinatorTest, DidFinishEditing) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:base_view_controller];

  AddressEditCoordinator* coordinator = [[AddressEditCoordinator alloc]
      initWithBaseViewController:base_view_controller];
  [coordinator setPaymentRequest:payment_request_.get()];

  // Set the address to be edited.
  autofill::AutofillProfile address;
  [coordinator setAddress:&address];

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

  // No autofill profile should get added to the PaymentRequest.
  EXPECT_CALL(*payment_request_, AddAutofillProfile(_)).Times(0);
  // No autofill profile should get added to the PersonalDataManager.
  EXPECT_CALL(personal_data_manager_, AddProfile(_)).Times(0);
  // Expect an autofill profile to be updated in the PersonalDataManager.
  EXPECT_CALL(personal_data_manager_,
              UpdateProfile(ProfileMatches("John Doe", "CA" /* Canada */,
                                           "Quebec", "16502111111")))
      .Times(1);

  // Call the controller delegate method.
  PaymentRequestEditViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator paymentRequestEditViewController:view_controller
                         didFinishEditingFields:GetEditorFields()];

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
  PaymentRequestEditViewController* view_controller =
      base::mac::ObjCCastStrict<PaymentRequestEditViewController>(
          navigation_controller.visibleViewController);
  [coordinator paymentRequestEditViewControllerDidCancel:view_controller];

  EXPECT_OCMOCK_VERIFY(delegate);
}
