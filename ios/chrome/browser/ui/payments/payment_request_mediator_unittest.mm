// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_mediator.h"

#import <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/core/strings_util.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#include "ios/chrome/browser/payments/payment_request_util.h"
#include "ios/chrome/browser/payments/test_payment_request.h"
#include "ios/chrome/browser/signin/fake_signin_manager_builder.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/payments/cells/autofill_profile_item.h"
#import "ios/chrome/browser/ui/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/payments/cells/price_item.h"
#include "ios/web/public/payments/payment_request.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payments::GetShippingOptionSectionString;
using ::payment_request_util::GetEmailLabelFromAutofillProfile;
using ::payment_request_util::GetNameLabelFromAutofillProfile;
using ::payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
using ::payment_request_util::GetShippingAddressLabelFromAutofillProfile;
}  // namespace

class PaymentRequestMediatorTest : public PlatformTest {
 protected:
  PaymentRequestMediatorTest()
      : autofill_profile_(autofill::test::GetFullProfile()),
        credit_card_(autofill::test::GetCreditCard()) {
    // Add testing profile and credit card to autofill::TestPersonalDataManager.
    personal_data_manager_.AddTestingProfile(&autofill_profile_);
    personal_data_manager_.AddTestingCreditCard(&credit_card_);

    payment_request_ = base::MakeUnique<TestPaymentRequest>(
        payment_request_test_util::CreateTestWebPaymentRequest(),
        &personal_data_manager_);

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(ios::SigninManagerFactory::GetInstance(),
                                       &ios::BuildFakeSigninManager);
    chrome_browser_state_ = test_cbs_builder.Build();
    mediator_ = [[PaymentRequestMediator alloc]
        initWithBrowserState:chrome_browser_state_.get()
              paymentRequest:payment_request_.get()];
  }

  PaymentRequestMediator* GetPaymentRequestMediator() { return mediator_; }

  web::TestWebThreadBundle thread_bundle_;

  autofill::AutofillProfile autofill_profile_;
  autofill::CreditCard credit_card_;
  autofill::TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<TestPaymentRequest> payment_request_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  PaymentRequestMediator* mediator_;
};

// Tests whether payment can be completed when expected.
TEST_F(PaymentRequestMediatorTest, TestCanPay) {
  // Payment cannot be completed if there is no selected credit card.
  EXPECT_TRUE([GetPaymentRequestMediator() canPay]);
  autofill::CreditCard* selected_credit_card =
      payment_request_->selected_credit_card();
  payment_request_->set_selected_credit_card(nullptr);
  EXPECT_FALSE([GetPaymentRequestMediator() canPay]);

  // Restore the selected credit card.
  payment_request_->set_selected_credit_card(selected_credit_card);
  EXPECT_TRUE([GetPaymentRequestMediator() canPay]);

  // Payment cannot be completed if there is no selected shipping profile,
  // unless no shipping information is requested.
  autofill::AutofillProfile* selected_shipping_profile =
      payment_request_->selected_shipping_profile();
  payment_request_->set_selected_shipping_profile(nullptr);
  EXPECT_FALSE([GetPaymentRequestMediator() canPay]);
  payment_request_->web_payment_request().options.request_shipping = false;
  EXPECT_FALSE([GetPaymentRequestMediator() requestShipping]);
  EXPECT_TRUE([GetPaymentRequestMediator() canPay]);

  // Restore the selected shipping profile and request for shipping information.
  payment_request_->set_selected_shipping_profile(selected_shipping_profile);
  payment_request_->web_payment_request().options.request_shipping = true;
  EXPECT_TRUE([GetPaymentRequestMediator() requestShipping]);
  EXPECT_TRUE([GetPaymentRequestMediator() canPay]);

  // Payment cannot be completed if there is no selected shipping option,
  // unless no shipping information is requested.
  web::PaymentShippingOption* selected_shipping_option =
      payment_request_->selected_shipping_option();
  payment_request_->set_selected_shipping_option(nullptr);
  EXPECT_FALSE([GetPaymentRequestMediator() canPay]);
  payment_request_->web_payment_request().options.request_shipping = false;
  EXPECT_TRUE([GetPaymentRequestMediator() canPay]);

  // Restore the selected shipping option and request for shipping information.
  payment_request_->set_selected_shipping_option(selected_shipping_option);
  payment_request_->web_payment_request().options.request_shipping = true;
  EXPECT_TRUE([GetPaymentRequestMediator() canPay]);

  // Payment cannot be completed if there is no selected contact profile, unless
  // no contact information is requested.
  payment_request_->set_selected_contact_profile(nullptr);
  EXPECT_FALSE([GetPaymentRequestMediator() canPay]);
  payment_request_->web_payment_request().options.request_payer_name = false;
  EXPECT_TRUE([GetPaymentRequestMediator() requestContactInfo]);
  EXPECT_FALSE([GetPaymentRequestMediator() canPay]);
  payment_request_->web_payment_request().options.request_payer_phone = false;
  EXPECT_TRUE([GetPaymentRequestMediator() requestContactInfo]);
  EXPECT_FALSE([GetPaymentRequestMediator() canPay]);
  payment_request_->web_payment_request().options.request_payer_email = false;
  EXPECT_FALSE([GetPaymentRequestMediator() requestContactInfo]);
  EXPECT_TRUE([GetPaymentRequestMediator() canPay]);
}

// Tests that the Payment Summary item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestPaymentSummaryItem) {
  EXPECT_TRUE([GetPaymentRequestMediator() hasPaymentItems]);

  // Payment Summary item should be of type PriceItem.
  id item = [GetPaymentRequestMediator() paymentSummaryItem];
  ASSERT_TRUE([item isMemberOfClass:[PriceItem class]]);
  PriceItem* payment_summary_item = base::mac::ObjCCastStrict<PriceItem>(item);
  EXPECT_TRUE([payment_summary_item.item isEqualToString:@"Total"]);
  EXPECT_TRUE([payment_summary_item.price isEqualToString:@"USD $1.00"]);
  EXPECT_EQ(nil, payment_summary_item.notification);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            payment_summary_item.accessoryType);

  // A label should indicate if the total value was changed.
  GetPaymentRequestMediator().totalValueChanged = YES;
  item = [GetPaymentRequestMediator() paymentSummaryItem];
  payment_summary_item = base::mac::ObjCCastStrict<PriceItem>(item);
  EXPECT_TRUE([payment_summary_item.notification
      isEqualToString:l10n_util::GetNSString(IDS_PAYMENTS_UPDATED_LABEL)]);

  // The next time the data source is queried for the Payment Summary item, the
  // label should disappear.
  item = [GetPaymentRequestMediator() paymentSummaryItem];
  payment_summary_item = base::mac::ObjCCastStrict<PriceItem>(item);
  EXPECT_EQ(nil, payment_summary_item.notification);

  // Remove the display items.
  web::PaymentRequest web_payment_request =
      payment_request_->web_payment_request();
  web_payment_request.details.display_items.clear();
  payment_request_->UpdatePaymentDetails(web_payment_request.details);
  EXPECT_FALSE([GetPaymentRequestMediator() hasPaymentItems]);

  // No accessory view indicates there are no display items.
  item = [GetPaymentRequestMediator() paymentSummaryItem];
  payment_summary_item = base::mac::ObjCCastStrict<PriceItem>(item);
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone,
            payment_summary_item.accessoryType);
}

// Tests that the Shipping section header item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestShippingHeaderItem) {
  // Shipping section header item should be of type PaymentsTextItem.
  id item = [GetPaymentRequestMediator() shippingSectionHeaderItem];
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
  PaymentsTextItem* shipping_section_header_item =
      base::mac::ObjCCastStrict<PaymentsTextItem>(item);
  EXPECT_TRUE([shipping_section_header_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_SHIPPING_SUMMARY_LABEL)]);
  EXPECT_EQ(nil, shipping_section_header_item.detailText);
}

// Tests that the Shipping Address item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestShippingAddressItem) {
  // Shipping Address item should be of type AutofillProfileItem.
  id item = [GetPaymentRequestMediator() shippingAddressItem];
  ASSERT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* shipping_address_item =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_TRUE([shipping_address_item.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request_->selected_shipping_profile())]);
  EXPECT_TRUE([shipping_address_item.address
      isEqualToString:GetShippingAddressLabelFromAutofillProfile(
                          *payment_request_->selected_shipping_profile())]);
  EXPECT_TRUE([shipping_address_item.phoneNumber
      isEqualToString:GetPhoneNumberLabelFromAutofillProfile(
                          *payment_request_->selected_shipping_profile())]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            shipping_address_item.accessoryType);

  // Reset the selected shipping profile.
  payment_request_->set_selected_shipping_profile(nullptr);

  // When there is no selected shipping address, the Shipping Address item
  // should be of type CollectionViewDetailItem.
  item = [GetPaymentRequestMediator() shippingAddressItem];
  ASSERT_TRUE([item isMemberOfClass:[CollectionViewDetailItem class]]);
  CollectionViewDetailItem* add_shipping_address_item =
      base::mac::ObjCCastStrict<CollectionViewDetailItem>(item);
  EXPECT_TRUE([add_shipping_address_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_SHIPPING_ADDRESS_LABEL)]);
  EXPECT_EQ(nil, add_shipping_address_item.detailText);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            add_shipping_address_item.accessoryType);

  // Remove the shipping profiles.
  payment_request_->ClearShippingProfiles();

  // No accessory view indicates there are no shipping profiles to choose from.
  item = [GetPaymentRequestMediator() shippingAddressItem];
  add_shipping_address_item =
      base::mac::ObjCCastStrict<CollectionViewDetailItem>(item);
  EXPECT_TRUE([add_shipping_address_item.detailText
      isEqualToString:[l10n_util::GetNSString(IDS_ADD)
                          uppercaseStringWithLocale:[NSLocale currentLocale]]]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone,
            add_shipping_address_item.accessoryType);
}

// Tests that the Shipping Option item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestShippingOptionItem) {
  // Shipping Option item should be of type PaymentsTextItem.
  id item = [GetPaymentRequestMediator() shippingOptionItem];
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
  PaymentsTextItem* shipping_option_item =
      base::mac::ObjCCastStrict<PaymentsTextItem>(item);
  EXPECT_TRUE([shipping_option_item.text isEqualToString:@"1-Day"]);
  EXPECT_TRUE([shipping_option_item.detailText isEqualToString:@"$0.99"]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            shipping_option_item.accessoryType);

  // Reset the selected shipping option.
  payment_request_->set_selected_shipping_option(nullptr);

  // When there is no selected shipping option, the Shipping Option item should
  // be of type CollectionViewDetailItem.
  item = [GetPaymentRequestMediator() shippingOptionItem];
  ASSERT_TRUE([item isMemberOfClass:[CollectionViewDetailItem class]]);
  CollectionViewDetailItem* add_shipping_option_item =
      base::mac::ObjCCastStrict<CollectionViewDetailItem>(item);
  EXPECT_TRUE([add_shipping_option_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_SHIPPING_OPTION_LABEL)]);
  EXPECT_EQ(nil, add_shipping_option_item.detailText);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            add_shipping_option_item.accessoryType);
}

// Tests that the Payment Method section header item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestPaymentMethodHeaderItem) {
  // Payment Method section header item should be of type PaymentsTextItem.
  id item = [GetPaymentRequestMediator() paymentMethodSectionHeaderItem];
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
  PaymentsTextItem* payment_method_section_header_item =
      base::mac::ObjCCastStrict<PaymentsTextItem>(item);
  EXPECT_TRUE([payment_method_section_header_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME)]);
  EXPECT_EQ(nil, payment_method_section_header_item.detailText);
}

// Tests that the Payment Method item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestPaymentMethodItem) {
  // Payment Method item should be of type PaymentsTextItem.
  id item = [GetPaymentRequestMediator() paymentMethodItem];
  ASSERT_TRUE([item isMemberOfClass:[PaymentMethodItem class]]);
  PaymentMethodItem* payment_method_item =
      base::mac::ObjCCastStrict<PaymentMethodItem>(item);
  EXPECT_TRUE([payment_method_item.methodID hasPrefix:@"Visa"]);
  EXPECT_TRUE([payment_method_item.methodID hasSuffix:@"1111"]);
  EXPECT_TRUE([payment_method_item.methodDetail isEqualToString:@"Test User"]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            payment_method_item.accessoryType);

  // Reset the selected credit card.
  payment_request_->set_selected_credit_card(nullptr);

  // When there is no selected credit card, the Payment Method item should be of
  // type CollectionViewDetailItem.
  item = [GetPaymentRequestMediator() paymentMethodItem];
  ASSERT_TRUE([item isMemberOfClass:[CollectionViewDetailItem class]]);
  CollectionViewDetailItem* add_payment_method_item =
      base::mac::ObjCCastStrict<CollectionViewDetailItem>(item);
  EXPECT_TRUE([add_payment_method_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME)]);
  EXPECT_EQ(nil, add_payment_method_item.detailText);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            add_payment_method_item.accessoryType);

  // Remove the credit cards.
  payment_request_->ClearCreditCards();

  // No accessory view indicates there are no payment methods to choose from.
  item = [GetPaymentRequestMediator() paymentMethodItem];
  add_payment_method_item =
      base::mac::ObjCCastStrict<CollectionViewDetailItem>(item);
  EXPECT_TRUE([add_payment_method_item.detailText
      isEqualToString:[l10n_util::GetNSString(IDS_ADD)
                          uppercaseStringWithLocale:[NSLocale currentLocale]]]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone,
            add_payment_method_item.accessoryType);
}

// Tests that the Contact Info section header item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestContactInfoHeaderItem) {
  // Contact Info section header item should be of type PaymentsTextItem.
  id item = [GetPaymentRequestMediator() contactInfoSectionHeaderItem];
  ASSERT_TRUE([item isMemberOfClass:[PaymentsTextItem class]]);
  PaymentsTextItem* contact_info_section_header_item =
      base::mac::ObjCCastStrict<PaymentsTextItem>(item);
  EXPECT_TRUE([contact_info_section_header_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_CONTACT_DETAILS_LABEL)]);
  EXPECT_EQ(nil, contact_info_section_header_item.detailText);
}

// Tests that the Contact Info item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestContactInfoItem) {
  // Contact Info item should be of type AutofillProfileItem.
  id item = [GetPaymentRequestMediator() contactInfoItem];
  ASSERT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* contact_info_item =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_TRUE([contact_info_item.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request_->selected_contact_profile())]);
  EXPECT_TRUE([contact_info_item.phoneNumber
      isEqualToString:GetPhoneNumberLabelFromAutofillProfile(
                          *payment_request_->selected_contact_profile())]);
  EXPECT_TRUE([contact_info_item.email
      isEqualToString:GetEmailLabelFromAutofillProfile(
                          *payment_request_->selected_contact_profile())]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            contact_info_item.accessoryType);

  // Contact Info item should only show requested fields.
  payment_request_->web_payment_request().options.request_payer_name = false;
  item = [GetPaymentRequestMediator() contactInfoItem];
  ASSERT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);
  contact_info_item = base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_EQ(nil, contact_info_item.name);
  EXPECT_NE(nil, contact_info_item.phoneNumber);
  EXPECT_NE(nil, contact_info_item.email);

  payment_request_->web_payment_request().options.request_payer_phone = false;
  item = [GetPaymentRequestMediator() contactInfoItem];
  ASSERT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);
  contact_info_item = base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_EQ(nil, contact_info_item.name);
  EXPECT_EQ(nil, contact_info_item.phoneNumber);
  EXPECT_NE(nil, contact_info_item.email);

  payment_request_->web_payment_request().options.request_payer_name = true;
  payment_request_->web_payment_request().options.request_payer_phone = false;
  payment_request_->web_payment_request().options.request_payer_email = false;
  item = [GetPaymentRequestMediator() contactInfoItem];
  ASSERT_TRUE([item isMemberOfClass:[AutofillProfileItem class]]);
  contact_info_item = base::mac::ObjCCastStrict<AutofillProfileItem>(item);
  EXPECT_NE(nil, contact_info_item.name);
  EXPECT_EQ(nil, contact_info_item.phoneNumber);
  EXPECT_EQ(nil, contact_info_item.email);

  // Reset the selected contact profile.
  payment_request_->set_selected_contact_profile(nullptr);

  // When there is no selected contact profile, the Payment Method item should
  // be of type CollectionViewDetailItem.
  item = [GetPaymentRequestMediator() contactInfoItem];
  ASSERT_TRUE([item isMemberOfClass:[CollectionViewDetailItem class]]);
  CollectionViewDetailItem* add_contact_info_item =
      base::mac::ObjCCastStrict<CollectionViewDetailItem>(item);
  EXPECT_TRUE([add_contact_info_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_CONTACT_DETAILS_LABEL)]);
  EXPECT_EQ(nil, add_contact_info_item.detailText);
  EXPECT_EQ(MDCCollectionViewCellAccessoryDisclosureIndicator,
            add_contact_info_item.accessoryType);

  // Remove the contact profiles.
  payment_request_->ClearContactProfiles();

  // No accessory view indicates there are no contact profiles to choose from.
  item = [GetPaymentRequestMediator() contactInfoItem];
  add_contact_info_item =
      base::mac::ObjCCastStrict<CollectionViewDetailItem>(item);
  EXPECT_TRUE([add_contact_info_item.detailText
      isEqualToString:[l10n_util::GetNSString(IDS_ADD)
                          uppercaseStringWithLocale:[NSLocale currentLocale]]]);
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone,
            add_contact_info_item.accessoryType);
}

// Tests that the Footer item is created as expected.
TEST_F(PaymentRequestMediatorTest, TestFooterItem) {
  // Make sure the user is signed out.
  SigninManager* signin_manager = ios::SigninManagerFactory::GetForBrowserState(
      chrome_browser_state_.get());
  if (signin_manager->IsAuthenticated()) {
    signin_manager->SignOut(signin_metrics::SIGNOUT_TEST,
                            signin_metrics::SignoutDelete::IGNORE_METRIC);
  }

  // Footer item should be of type CollectionViewFooterItem.
  id item = [GetPaymentRequestMediator() footerItem];
  ASSERT_TRUE([item isMemberOfClass:[CollectionViewFooterItem class]]);
  CollectionViewFooterItem* footer_item =
      base::mac::ObjCCastStrict<CollectionViewFooterItem>(item);
  EXPECT_TRUE([footer_item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_PAYMENTS_CARD_AND_ADDRESS_SETTINGS_SIGNED_OUT)]);

  // Fake a signed in user.
  signin_manager->SetAuthenticatedAccountInfo("12345", "username@example.com");

  item = [GetPaymentRequestMediator() footerItem];
  footer_item = base::mac::ObjCCastStrict<CollectionViewFooterItem>(item);
  EXPECT_TRUE([footer_item.text
      isEqualToString:l10n_util::GetNSStringF(
                          IDS_PAYMENTS_CARD_AND_ADDRESS_SETTINGS_SIGNED_IN,
                          base::ASCIIToUTF16("username@example.com"))]);
}
