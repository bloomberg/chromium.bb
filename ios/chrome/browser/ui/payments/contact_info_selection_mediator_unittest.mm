// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/contact_info_selection_mediator.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#include "ios/chrome/browser/payments/test_payment_request.h"
#import "ios/chrome/browser/ui/payments/cells/autofill_profile_item.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payment_request_util::GetNameLabelFromAutofillProfile;
using ::payment_request_util::GetEmailLabelFromAutofillProfile;
using ::payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
using ::payment_request_util::GetContactNotificationLabelFromAutofillProfile;
}  // namespace

class PaymentRequestContactInfoSelectionMediatorTest : public PlatformTest {
 protected:
  PaymentRequestContactInfoSelectionMediatorTest()
      : autofill_profile_1_(autofill::test::GetFullProfile()),
        autofill_profile_2_(autofill::test::GetFullProfile2()),
        autofill_profile_3_(autofill::test::GetFullProfile()),
        chrome_browser_state_(TestChromeBrowserState::Builder().Build()) {
    // Change the name of the third profile (to avoid deduplication), and make
    // it incomplete by removing the phone number.
    autofill_profile_3_.SetInfo(autofill::AutofillType(autofill::NAME_FULL),
                                base::ASCIIToUTF16("Richard Roe"), "en-US");
    autofill_profile_3_.SetInfo(
        autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
        base::string16(), "en-US");

    // Add testing profiles to autofill::TestPersonalDataManager.
    personal_data_manager_.AddTestingProfile(&autofill_profile_1_);
    personal_data_manager_.AddTestingProfile(&autofill_profile_2_);
    personal_data_manager_.AddTestingProfile(&autofill_profile_3_);
    payment_request_ = base::MakeUnique<payments::TestPaymentRequest>(
        payment_request_test_util::CreateTestWebPaymentRequest(),
        chrome_browser_state_.get(), &web_state_, &personal_data_manager_);

    // Override the selected contact profile.
    payment_request_->set_selected_contact_profile(
        payment_request_->contact_profiles()[1]);
  }

  void SetUp() override {
    mediator_ = [[ContactInfoSelectionMediator alloc]
        initWithPaymentRequest:payment_request_.get()];
  }

  ContactInfoSelectionMediator* GetMediator() { return mediator_; }

  base::test::ScopedTaskEnvironment scoped_task_evironment_;

  ContactInfoSelectionMediator* mediator_;

  autofill::AutofillProfile autofill_profile_1_;
  autofill::AutofillProfile autofill_profile_2_;
  autofill::AutofillProfile autofill_profile_3_;
  web::TestWebState web_state_;
  autofill::TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<payments::TestPaymentRequest> payment_request_;
};

// Tests that the expected selectable items are created and that the index of
// the selected item is properly set.
TEST_F(PaymentRequestContactInfoSelectionMediatorTest, TestSelectableItems) {
  NSArray<CollectionViewItem*>* selectable_items =
      [GetMediator() selectableItems];

  // There must be three selectable items.
  ASSERT_EQ(3U, selectable_items.count);

  // The second item must be selected.
  EXPECT_EQ(1U, GetMediator().selectedItemIndex);

  CollectionViewItem* item_1 = [selectable_items objectAtIndex:0];
  DCHECK([item_1 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_1 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_1);
  EXPECT_TRUE([profile_item_1.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request_->contact_profiles()[0])]);
  EXPECT_TRUE([profile_item_1.email
      isEqualToString:GetEmailLabelFromAutofillProfile(
                          *payment_request_->contact_profiles()[0])]);
  EXPECT_TRUE([profile_item_1.phoneNumber
      isEqualToString:GetPhoneNumberLabelFromAutofillProfile(
                          *payment_request_->contact_profiles()[0])]);
  EXPECT_EQ(nil, profile_item_1.address);
  EXPECT_EQ(nil, profile_item_1.notification);

  CollectionViewItem* item_2 = [selectable_items objectAtIndex:1];
  DCHECK([item_2 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_2 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_2);
  EXPECT_TRUE([profile_item_2.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request_->contact_profiles()[1])]);
  EXPECT_TRUE([profile_item_2.email
      isEqualToString:GetEmailLabelFromAutofillProfile(
                          *payment_request_->contact_profiles()[1])]);
  EXPECT_TRUE([profile_item_2.phoneNumber
      isEqualToString:GetPhoneNumberLabelFromAutofillProfile(
                          *payment_request_->contact_profiles()[1])]);
  EXPECT_EQ(nil, profile_item_2.address);
  EXPECT_EQ(nil, profile_item_2.notification);

  CollectionViewItem* item_3 = [selectable_items objectAtIndex:2];
  DCHECK([item_3 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_3 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_3);
  EXPECT_TRUE([profile_item_3.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request_->contact_profiles()[2])]);
  EXPECT_TRUE([profile_item_3.email
      isEqualToString:GetEmailLabelFromAutofillProfile(
                          *payment_request_->contact_profiles()[2])]);
  EXPECT_EQ(nil, profile_item_3.phoneNumber);
  EXPECT_EQ(nil, profile_item_3.address);
  EXPECT_TRUE([profile_item_3.notification
      isEqualToString:GetContactNotificationLabelFromAutofillProfile(
                          *payment_request_.get(),
                          *payment_request_->contact_profiles()[2])]);
}

// Tests that the index of the selected item is as expected when there is no
// selected contact profile.
TEST_F(PaymentRequestContactInfoSelectionMediatorTest, TestNoSelectedItem) {
  // Reset the selected contact profile.
  payment_request_->set_selected_contact_profile(nullptr);
  mediator_ = [[ContactInfoSelectionMediator alloc]
      initWithPaymentRequest:payment_request_.get()];

  NSArray<CollectionViewItem*>* selectable_items =
      [GetMediator() selectableItems];

  // There must be three selectable items.
  ASSERT_EQ(3U, selectable_items.count);

  // The selected item index must be invalid.
  EXPECT_EQ(NSUIntegerMax, GetMediator().selectedItemIndex);
}

// Tests that only the requested fields are displayed.
TEST_F(PaymentRequestContactInfoSelectionMediatorTest, TestOnlyRequestedData) {
  // Update the web_payment_request and reload the items.
  payment_request_->web_payment_request().options.request_payer_name = false;
  [GetMediator() loadItems];

  NSArray<CollectionViewItem*>* selectable_items =
      [GetMediator() selectableItems];

  CollectionViewItem* item = [selectable_items objectAtIndex:0];
  DCHECK([item isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_EQ(nil, profile_item.name);
  EXPECT_NE(nil, profile_item.email);
  EXPECT_NE(nil, profile_item.phoneNumber);
  EXPECT_EQ(nil, profile_item.address);
  EXPECT_EQ(nil, profile_item.notification);

  // Incomplete item should display a notification since the phone number is
  // missing.
  CollectionViewItem* incomplete_item = [selectable_items objectAtIndex:2];
  DCHECK([incomplete_item isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* incomplete_profile_item =
      base::mac::ObjCCastStrict<AutofillProfileItem>(incomplete_item);
  EXPECT_NE(nil, incomplete_profile_item.notification);

  // Update the web_payment_request and reload the items.
  payment_request_->web_payment_request().options.request_payer_email = false;
  [GetMediator() loadItems];

  selectable_items = [GetMediator() selectableItems];

  item = [selectable_items objectAtIndex:0];
  DCHECK([item isKindOfClass:[AutofillProfileItem class]]);
  profile_item = base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_EQ(nil, profile_item.name);
  EXPECT_EQ(nil, profile_item.email);
  EXPECT_NE(nil, profile_item.phoneNumber);
  EXPECT_EQ(nil, profile_item.address);
  EXPECT_EQ(nil, profile_item.notification);

  // Update the web_payment_request and reload the items.
  payment_request_->web_payment_request().options.request_payer_name = true;
  payment_request_->web_payment_request().options.request_payer_email = true;
  payment_request_->web_payment_request().options.request_payer_phone = false;
  [GetMediator() loadItems];

  selectable_items = [GetMediator() selectableItems];

  item = [selectable_items objectAtIndex:0];
  DCHECK([item isKindOfClass:[AutofillProfileItem class]]);
  profile_item = base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_NE(nil, profile_item.name);
  EXPECT_NE(nil, profile_item.email);
  EXPECT_EQ(nil, profile_item.phoneNumber);
  EXPECT_EQ(nil, profile_item.address);
  EXPECT_EQ(nil, profile_item.notification);

  // Incomplete item should not display a notification, since the phone number
  // is not requested.
  incomplete_item = [selectable_items objectAtIndex:2];
  DCHECK([incomplete_item isKindOfClass:[AutofillProfileItem class]]);
  profile_item =
      base::mac::ObjCCastStrict<AutofillProfileItem>(incomplete_item);
  EXPECT_EQ(nil, profile_item.notification);
}
