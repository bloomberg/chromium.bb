// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/payment_request.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/core/autofill_payment_instrument.h"
#include "components/payments/core/currency_formatter.h"
#include "components/payments/core/features.h"
#include "components/payments/core/payment_details.h"
#include "components/payments/core/payment_method_data.h"
#include "components/payments/core/payment_options.h"
#include "components/payments/core/payment_shipping_option.h"
#include "components/payments/core/web_payment_request.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#include "ios/chrome/browser/payments/test_payment_request.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class MockTestPersonalDataManager : public autofill::TestPersonalDataManager {
 public:
  MockTestPersonalDataManager() : TestPersonalDataManager() {}
  MOCK_METHOD1(RecordUseOf, void(const autofill::AutofillDataModel&));
};

MATCHER_P(GuidMatches, guid, "") {
  return arg.guid() == guid;
}

}  // namespace

namespace payments {

class PaymentRequestTest : public PlatformTest {
 protected:
  PaymentRequestTest()
      : chrome_browser_state_(TestChromeBrowserState::Builder().Build()) {}

  // Returns PaymentDetails with one shipping option that's selected.
  PaymentDetails CreateDetailsWithShippingOption() {
    PaymentDetails details;
    std::vector<PaymentShippingOption> shipping_options;
    PaymentShippingOption option1;
    option1.id = "option:1";
    option1.selected = true;
    shipping_options.push_back(std::move(option1));
    details.shipping_options = std::move(shipping_options);

    return details;
  }

  PaymentOptions CreatePaymentOptions(bool request_payer_name,
                                      bool request_payer_phone,
                                      bool request_payer_email,
                                      bool request_shipping) {
    PaymentOptions options;
    options.request_payer_name = request_payer_name;
    options.request_payer_phone = request_payer_phone;
    options.request_payer_email = request_payer_email;
    options.request_shipping = request_shipping;
    return options;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  web::TestWebState web_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Tests that the CurrencyFormatter is constructed with the correct
// currency code and currency system.
TEST_F(PaymentRequestTest, CreatesCurrencyFormatterCorrectly) {
  WebPaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  web_payment_request.details.total = base::MakeUnique<PaymentItem>();
  web_payment_request.details.total->amount.currency = "USD";
  TestPaymentRequest payment_request1(web_payment_request,
                                      chrome_browser_state_.get(), &web_state_,
                                      &personal_data_manager);
  ASSERT_EQ("en", payment_request1.GetApplicationLocale());
  CurrencyFormatter* currency_formatter =
      payment_request1.GetOrCreateCurrencyFormatter();
  EXPECT_EQ(base::UTF8ToUTF16("$55.00"), currency_formatter->Format("55.00"));
  EXPECT_EQ("USD", currency_formatter->formatted_currency_code());

  web_payment_request.details.total->amount.currency = "JPY";
  TestPaymentRequest payment_request2(web_payment_request,
                                      chrome_browser_state_.get(), &web_state_,
                                      &personal_data_manager);
  ASSERT_EQ("en", payment_request2.GetApplicationLocale());
  currency_formatter = payment_request2.GetOrCreateCurrencyFormatter();
  EXPECT_EQ(base::UTF8ToUTF16("¥55"), currency_formatter->Format("55.00"));
  EXPECT_EQ("JPY", currency_formatter->formatted_currency_code());

  web_payment_request.details.total->amount.currency_system = "NOT_ISO4217";
  web_payment_request.details.total->amount.currency = "USD";
  TestPaymentRequest payment_request3(web_payment_request,
                                      chrome_browser_state_.get(), &web_state_,
                                      &personal_data_manager);
  ASSERT_EQ("en", payment_request3.GetApplicationLocale());
  currency_formatter = payment_request3.GetOrCreateCurrencyFormatter();
  EXPECT_EQ(base::UTF8ToUTF16("55.00"), currency_formatter->Format("55.00"));
  EXPECT_EQ("USD", currency_formatter->formatted_currency_code());
}

// Tests that the accepted card networks are identified correctly.
TEST_F(PaymentRequestTest, AcceptedPaymentNetworks) {
  WebPaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  PaymentMethodData method_datum1;
  method_datum1.supported_methods.push_back("visa");
  web_payment_request.method_data.push_back(method_datum1);
  PaymentMethodData method_datum2;
  method_datum2.supported_methods.push_back("mastercard");
  web_payment_request.method_data.push_back(method_datum2);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  ASSERT_EQ(2U, payment_request.supported_card_networks().size());
  EXPECT_EQ("visa", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("mastercard", payment_request.supported_card_networks()[1]);
}

// Test that parsing supported methods (with invalid values and duplicates)
// works as expected.
TEST_F(PaymentRequestTest, SupportedMethods) {
  WebPaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebPaymentsNativeApps);

  PaymentMethodData method_datum1;
  method_datum1.supported_methods.push_back("visa");
  method_datum1.supported_methods.push_back("mastercard");
  method_datum1.supported_methods.push_back("invalid");
  method_datum1.supported_methods.push_back("");
  method_datum1.supported_methods.push_back("visa");
  method_datum1.supported_methods.push_back("https://bobpay.com");
  method_datum1.supported_methods.push_back("http://invalidpay.com");
  web_payment_request.method_data.push_back(method_datum1);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  payment_request.ResetParsedPaymentMethodData();
  ASSERT_EQ(2U, payment_request.supported_card_networks().size());
  EXPECT_EQ("visa", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("mastercard", payment_request.supported_card_networks()[1]);
  ASSERT_EQ(1U, payment_request.url_payment_method_identifiers().size());
  EXPECT_EQ(GURL("https://bobpay.com"),
            payment_request.url_payment_method_identifiers()[0]);
}

// Test that parsing supported methods in different method data entries (with
// invalid values and duplicates) works as expected.
TEST_F(PaymentRequestTest, SupportedMethods_MultipleEntries) {
  WebPaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebPaymentsNativeApps);

  PaymentMethodData method_datum1;
  method_datum1.supported_methods.push_back("visa");
  method_datum1.supported_methods.push_back("https://bobpay.com");
  web_payment_request.method_data.push_back(method_datum1);
  PaymentMethodData method_datum2;
  method_datum2.supported_methods.push_back("mastercard");
  web_payment_request.method_data.push_back(method_datum2);
  PaymentMethodData method_datum3;
  method_datum3.supported_methods.push_back("");
  method_datum3.supported_methods.push_back("http://invalidpay.com");
  web_payment_request.method_data.push_back(method_datum3);
  PaymentMethodData method_datum4;
  method_datum4.supported_methods.push_back("visa");
  method_datum4.supported_methods.push_back("https://bobpay.com");
  web_payment_request.method_data.push_back(method_datum4);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  payment_request.ResetParsedPaymentMethodData();
  ASSERT_EQ(2U, payment_request.supported_card_networks().size());
  EXPECT_EQ("visa", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("mastercard", payment_request.supported_card_networks()[1]);
  ASSERT_EQ(1U, payment_request.url_payment_method_identifiers().size());
  EXPECT_EQ(GURL("https://bobpay.com"),
            payment_request.url_payment_method_identifiers()[0]);
}

// Test that only specifying basic-card means that all are supported.
TEST_F(PaymentRequestTest, SupportedMethods_OnlyBasicCard) {
  WebPaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  PaymentMethodData method_datum1;
  method_datum1.supported_methods.push_back("basic-card");
  web_payment_request.method_data.push_back(method_datum1);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);

  // All of the basic card networks are supported.
  ASSERT_EQ(8U, payment_request.supported_card_networks().size());
  EXPECT_EQ("amex", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("diners", payment_request.supported_card_networks()[1]);
  EXPECT_EQ("discover", payment_request.supported_card_networks()[2]);
  EXPECT_EQ("jcb", payment_request.supported_card_networks()[3]);
  EXPECT_EQ("mastercard", payment_request.supported_card_networks()[4]);
  EXPECT_EQ("mir", payment_request.supported_card_networks()[5]);
  EXPECT_EQ("unionpay", payment_request.supported_card_networks()[6]);
  EXPECT_EQ("visa", payment_request.supported_card_networks()[7]);

  EXPECT_TRUE(payment_request.url_payment_method_identifiers().empty());
}

// Test that specifying a method AND basic-card means that all are supported,
// but with the method as first.
TEST_F(PaymentRequestTest, SupportedMethods_BasicCard_WithSpecificMethod) {
  WebPaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  PaymentMethodData method_datum1;
  method_datum1.supported_methods.push_back("jcb");
  method_datum1.supported_methods.push_back("basic-card");
  web_payment_request.method_data.push_back(method_datum1);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);

  // All of the basic card networks are supported, but JCB is first because it
  // was specified first.
  EXPECT_EQ(8u, payment_request.supported_card_networks().size());
  EXPECT_EQ("jcb", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("amex", payment_request.supported_card_networks()[1]);
  EXPECT_EQ("diners", payment_request.supported_card_networks()[2]);
  EXPECT_EQ("discover", payment_request.supported_card_networks()[3]);
  EXPECT_EQ("mastercard", payment_request.supported_card_networks()[4]);
  EXPECT_EQ("mir", payment_request.supported_card_networks()[5]);
  EXPECT_EQ("unionpay", payment_request.supported_card_networks()[6]);
  EXPECT_EQ("visa", payment_request.supported_card_networks()[7]);
}

// Test that specifying basic-card with a supported network (with previous
// supported methods) will work as expected
TEST_F(PaymentRequestTest, SupportedMethods_BasicCard_Overlap) {
  WebPaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  PaymentMethodData method_datum1;
  method_datum1.supported_methods.push_back("mastercard");
  method_datum1.supported_methods.push_back("visa");
  web_payment_request.method_data.push_back(method_datum1);
  PaymentMethodData method_datum2;
  method_datum2.supported_methods.push_back("basic-card");
  method_datum2.supported_networks.push_back("visa");
  method_datum2.supported_networks.push_back("mastercard");
  method_datum2.supported_networks.push_back("unionpay");
  web_payment_request.method_data.push_back(method_datum2);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);

  EXPECT_EQ(3u, payment_request.supported_card_networks().size());
  EXPECT_EQ("mastercard", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("visa", payment_request.supported_card_networks()[1]);
  EXPECT_EQ("unionpay", payment_request.supported_card_networks()[2]);
}

// Test that specifying basic-card with supported networks after specifying
// some methods
TEST_F(PaymentRequestTest, SupportedMethods_BasicCard_WithSupportedNetworks) {
  WebPaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  PaymentMethodData method_datum1;
  method_datum1.supported_methods.push_back("basic-card");
  method_datum1.supported_networks.push_back("visa");
  method_datum1.supported_networks.push_back("unionpay");
  web_payment_request.method_data.push_back(method_datum1);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);

  // Only the specified networks are supported.
  EXPECT_EQ(2u, payment_request.supported_card_networks().size());
  EXPECT_EQ("visa", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("unionpay", payment_request.supported_card_networks()[1]);
}

// Tests that an autofill payment instrumnt e.g., credit cards can be added
// to the list of available payment methods.
TEST_F(PaymentRequestTest, AddAutofillPaymentInstrument) {
  WebPaymentRequest web_payment_request;
  PaymentMethodData method_datum;
  method_datum.supported_methods.push_back("basic-card");
  method_datum.supported_networks.push_back("visa");
  method_datum.supported_networks.push_back("amex");
  web_payment_request.method_data.push_back(method_datum);

  autofill::TestPersonalDataManager personal_data_manager;

  autofill::CreditCard credit_card_1 = autofill::test::GetCreditCard();
  personal_data_manager.AddTestingCreditCard(&credit_card_1);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  EXPECT_EQ(1U, payment_request.payment_methods().size());

  autofill::CreditCard credit_card_2 = autofill::test::GetCreditCard2();
  AutofillPaymentInstrument* added_credit_card =
      payment_request.AddAutofillPaymentInstrument(credit_card_2);

  EXPECT_EQ(2U, payment_request.payment_methods().size());
  EXPECT_EQ(credit_card_2, *added_credit_card->credit_card());
}

// Tests that a profile can be added to the list of available profiles.
TEST_F(PaymentRequestTest, AddAutofillProfile) {
  WebPaymentRequest web_payment_request;
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  autofill::TestPersonalDataManager personal_data_manager;

  autofill::AutofillProfile profile_1 = autofill::test::GetFullProfile();
  personal_data_manager.AddTestingProfile(&profile_1);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  EXPECT_EQ(1U, payment_request.shipping_profiles().size());
  EXPECT_EQ(1U, payment_request.contact_profiles().size());

  autofill::AutofillProfile profile_2 = autofill::test::GetFullProfile2();
  autofill::AutofillProfile* added_profile =
      payment_request.AddAutofillProfile(profile_2);

  EXPECT_EQ(2U, payment_request.shipping_profiles().size());
  EXPECT_EQ(2U, payment_request.contact_profiles().size());
  EXPECT_EQ(profile_2, *added_profile);
}

// Test that parsing shipping options works as expected.
TEST_F(PaymentRequestTest, SelectedShippingOptions) {
  WebPaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  PaymentDetails details;
  details.total = base::MakeUnique<PaymentItem>();
  std::vector<PaymentShippingOption> shipping_options;
  PaymentShippingOption option1;
  option1.id = "option:1";
  option1.selected = false;
  shipping_options.push_back(std::move(option1));
  PaymentShippingOption option2;
  option2.id = "option:2";
  option2.selected = true;
  shipping_options.push_back(std::move(option2));
  PaymentShippingOption option3;
  option3.id = "option:3";
  option3.selected = true;
  shipping_options.push_back(std::move(option3));
  details.shipping_options = std::move(shipping_options);
  web_payment_request.details = std::move(details);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  // The last one marked "selected" should be selected.
  EXPECT_EQ("option:3", payment_request.selected_shipping_option()->id);

  // Simulate an update that no longer has any shipping options. There is no
  // longer a selected shipping option.
  PaymentDetails new_details;
  payment_request.UpdatePaymentDetails(std::move(new_details));
  EXPECT_EQ(nullptr, payment_request.selected_shipping_option());
}

// Tests that updating the payment details updates the total amount.
TEST_F(PaymentRequestTest, UpdatePaymentDetailsNewTotal) {
  WebPaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  PaymentDetails details;
  details.total = base::MakeUnique<PaymentItem>();
  details.total->amount.value = "10.00";
  details.total->amount.currency = "USD";
  web_payment_request.details = std::move(details);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);

  // Simulate an update with a new total amount.
  PaymentDetails new_details;
  new_details.total = base::MakeUnique<PaymentItem>();
  new_details.total->amount.value = "20.00";
  new_details.total->amount.currency = "CAD";
  payment_request.UpdatePaymentDetails(std::move(new_details));
  EXPECT_EQ("20.00", payment_request.payment_details().total->amount.value);
  EXPECT_EQ("CAD", payment_request.payment_details().total->amount.currency);
}

// Tests that updating the payment details with a PaymentDetails instance that
// is missing the total amount, maintains the old total amount.
TEST_F(PaymentRequestTest, UpdatePaymentDetailsNoTotal) {
  WebPaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  PaymentDetails details;
  details.total = base::MakeUnique<PaymentItem>();
  details.total->amount.value = "10.00";
  details.total->amount.currency = "USD";
  web_payment_request.details = std::move(details);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);

  // Simulate an update with the total amount missing.
  PaymentDetails new_details;
  payment_request.UpdatePaymentDetails(std::move(new_details));
  EXPECT_EQ("10.00", payment_request.payment_details().total->amount.value);
  EXPECT_EQ("USD", payment_request.payment_details().total->amount.currency);
}

// Test that loading profiles when none are available works as expected.
TEST_F(PaymentRequestTest, SelectedProfiles_NoProfiles) {
  autofill::TestPersonalDataManager personal_data_manager;
  WebPaymentRequest web_payment_request;
  web_payment_request.details = CreateDetailsWithShippingOption();
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  // No profiles are selected because none are available!
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  EXPECT_EQ(nullptr, payment_request.selected_shipping_profile());
  EXPECT_EQ(nullptr, payment_request.selected_contact_profile());
}

// Test that loading complete shipping and contact profiles works as expected.
TEST_F(PaymentRequestTest, SelectedProfiles_Complete) {
  autofill::TestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.set_use_count(5U);
  personal_data_manager.AddTestingProfile(&address);
  autofill::AutofillProfile address2 = autofill::test::GetFullProfile2();
  address2.set_use_count(15U);
  personal_data_manager.AddTestingProfile(&address2);

  WebPaymentRequest web_payment_request;
  web_payment_request.details = CreateDetailsWithShippingOption();
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  // address2 is selected because it has the most use count (Frecency model).
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  EXPECT_EQ(address2.guid(),
            payment_request.selected_shipping_profile()->guid());
  EXPECT_EQ(address2.guid(),
            payment_request.selected_contact_profile()->guid());
}

// Test that loading complete shipping and contact profiles, when there are no
// shipping options available, works as expected.
TEST_F(PaymentRequestTest, SelectedProfiles_Complete_NoShippingOption) {
  autofill::TestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.set_use_count(5U);
  personal_data_manager.AddTestingProfile(&address);

  WebPaymentRequest web_payment_request;
  // No shipping options.
  web_payment_request.details = PaymentDetails();
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  // No shipping profile is selected because the merchant has not selected a
  // shipping option. However there is a suitable contact profile.
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  EXPECT_EQ(nullptr, payment_request.selected_shipping_profile());
  EXPECT_EQ(address.guid(), payment_request.selected_contact_profile()->guid());
}

// Test that loading incomplete shipping and contact profiles works as expected.
TEST_F(PaymentRequestTest, SelectedProfiles_Incomplete) {
  autofill::TestPersonalDataManager personal_data_manager;
  // Add a profile with no phone (incomplete).
  autofill::AutofillProfile address1 = autofill::test::GetFullProfile();
  address1.SetInfo(autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
                   base::string16(), "en-US");
  address1.set_use_count(5U);
  personal_data_manager.AddTestingProfile(&address1);
  // Add a complete profile, with fewer use counts.
  autofill::AutofillProfile address2 = autofill::test::GetFullProfile2();
  address2.set_use_count(3U);
  personal_data_manager.AddTestingProfile(&address2);

  WebPaymentRequest web_payment_request;
  web_payment_request.details = CreateDetailsWithShippingOption();
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  // Even though address1 has more use counts, address2 is selected because it
  // is complete.
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  EXPECT_EQ(address2.guid(),
            payment_request.selected_shipping_profile()->guid());
  EXPECT_EQ(address2.guid(),
            payment_request.selected_contact_profile()->guid());
}

// Test that loading incomplete contact profiles works as expected when the
// merchant is not interested in the missing field. Test that the most complete
// shipping profile is selected.
TEST_F(PaymentRequestTest,
       SelectedProfiles_IncompleteContact_NoRequestPayerPhone) {
  autofill::TestPersonalDataManager personal_data_manager;
  // Add a profile with no phone (incomplete).
  autofill::AutofillProfile address1 = autofill::test::GetFullProfile();
  address1.SetInfo(autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
                   base::string16(), "en-US");
  address1.set_use_count(5U);
  personal_data_manager.AddTestingProfile(&address1);
  // Add a complete profile, with fewer use counts.
  autofill::AutofillProfile address2 = autofill::test::GetFullProfile();
  address2.set_use_count(3U);
  personal_data_manager.AddTestingProfile(&address2);

  WebPaymentRequest web_payment_request;
  web_payment_request.details = CreateDetailsWithShippingOption();
  // The merchant doesn't care about the phone number.
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/false,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  // address1 has more use counts, and even though it has no phone number, it's
  // still selected as the contact profile because merchant doesn't require
  // phone. address2 is selected as the shipping profile because it's the most
  // complete for shipping.
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  EXPECT_EQ(address2.guid(),
            payment_request.selected_shipping_profile()->guid());
  EXPECT_EQ(address1.guid(),
            payment_request.selected_contact_profile()->guid());
}

// Test that loading payment methods when none are available works as expected.
TEST_F(PaymentRequestTest, SelectedPaymentMethod_NoPaymentMethods) {
  autofill::TestPersonalDataManager personal_data_manager;
  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();

  // No payment methods are selected because none are available!
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  EXPECT_EQ(nullptr, payment_request.selected_payment_method());
}

// Test that loading expired credit cards works as expected.
TEST_F(PaymentRequestTest, SelectedPaymentMethod_ExpiredCard) {
  autofill::TestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  personal_data_manager.AddTestingProfile(&billing_address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  personal_data_manager.AddTestingCreditCard(&credit_card);
  credit_card.SetExpirationYear(2016);  // Expired.
  credit_card.set_billing_address_id(billing_address.guid());

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();

  // credit_card is selected because expired cards are valid for payment.
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  EXPECT_EQ(payment_request.selected_payment_method()->type(),
            PaymentInstrument::Type::AUTOFILL);
  AutofillPaymentInstrument* payment_instrument =
      static_cast<AutofillPaymentInstrument*>(
          payment_request.selected_payment_method());
  EXPECT_EQ(credit_card.guid(), payment_instrument->credit_card()->guid());
}

// Test that loading complete payment methods works as expected.
TEST_F(PaymentRequestTest, SelectedPaymentMethod_Complete) {
  autofill::TestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  personal_data_manager.AddTestingProfile(&billing_address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  credit_card.set_use_count(5U);
  personal_data_manager.AddTestingCreditCard(&credit_card);
  credit_card.set_billing_address_id(billing_address.guid());
  autofill::CreditCard credit_card2 = autofill::test::GetCreditCard2();
  credit_card2.set_use_count(15U);
  personal_data_manager.AddTestingCreditCard(&credit_card2);
  credit_card2.set_billing_address_id(billing_address.guid());

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();

  // credit_card2 is selected because it has the most use count (Frecency
  // model).
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentInstrument* payment_instrument =
      static_cast<AutofillPaymentInstrument*>(
          payment_request.selected_payment_method());
  EXPECT_EQ(credit_card2.guid(), payment_instrument->credit_card()->guid());
}

// Test that loading incomplete payment methods works as expected.
TEST_F(PaymentRequestTest, SelectedPaymentMethod_Incomplete) {
  autofill::TestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  personal_data_manager.AddTestingProfile(&billing_address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  credit_card.set_use_count(5U);
  personal_data_manager.AddTestingCreditCard(&credit_card);
  credit_card.set_billing_address_id(billing_address.guid());
  autofill::CreditCard credit_card2 = autofill::test::GetCreditCard2();
  credit_card2.set_use_count(15U);
  personal_data_manager.AddTestingCreditCard(&credit_card2);

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();

  // Even though credit_card2 has more use counts, credit_card is selected
  // because it is complete.
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentInstrument* payment_instrument =
      static_cast<AutofillPaymentInstrument*>(
          payment_request.selected_payment_method());
  EXPECT_EQ(credit_card.guid(), payment_instrument->credit_card()->guid());
}

// Test that the use counts of the data models are updated as expected when
// different autofill profiles are used as the shipping address and the contact
// info.
TEST_F(PaymentRequestTest, RecordUseStats_RequestShippingAndContactInfo) {
  MockTestPersonalDataManager personal_data_manager;
  // Add a profile that is incomplete for a contact info, but is used more
  // frequently so is selected as the default shipping address.
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  address.SetInfo(autofill::AutofillType(autofill::EMAIL_ADDRESS),
                  base::string16(), "en-US");
  address.set_use_count(10U);
  personal_data_manager.AddTestingProfile(&address);
  autofill::AutofillProfile contact_info = autofill::test::GetFullProfile2();
  contact_info.set_use_count(5U);
  personal_data_manager.AddTestingProfile(&contact_info);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  personal_data_manager.AddTestingCreditCard(&credit_card);
  credit_card.set_billing_address_id(address.guid());

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentInstrument* payment_instrument =
      static_cast<AutofillPaymentInstrument*>(
          payment_request.selected_payment_method());
  EXPECT_EQ(address.guid(),
            payment_request.selected_shipping_profile()->guid());
  EXPECT_EQ(contact_info.guid(),
            payment_request.selected_contact_profile()->guid());
  EXPECT_EQ(credit_card.guid(), payment_instrument->credit_card()->guid());

  EXPECT_CALL(personal_data_manager, RecordUseOf(GuidMatches(address.guid())))
      .Times(1);
  EXPECT_CALL(personal_data_manager,
              RecordUseOf(GuidMatches(contact_info.guid())))
      .Times(1);
  EXPECT_CALL(personal_data_manager,
              RecordUseOf(GuidMatches(credit_card.guid())))
      .Times(1);

  payment_request.RecordUseStats();
}

// Test that the use counts of the data models are updated as expected when the
// same autofill profile is used as the shipping address and the contact info.
TEST_F(PaymentRequestTest, RecordUseStats_SameShippingAndContactInfoProfile) {
  MockTestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  personal_data_manager.AddTestingProfile(&address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  personal_data_manager.AddTestingCreditCard(&credit_card);
  credit_card.set_billing_address_id(address.guid());

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentInstrument* payment_instrument =
      static_cast<AutofillPaymentInstrument*>(
          payment_request.selected_payment_method());
  EXPECT_EQ(address.guid(),
            payment_request.selected_shipping_profile()->guid());
  EXPECT_EQ(address.guid(), payment_request.selected_contact_profile()->guid());
  EXPECT_EQ(credit_card.guid(), payment_instrument->credit_card()->guid());

  // Even though |address| is used for contact info, shipping address, and
  // credit_card's billing address, the stats should be updated only once.
  EXPECT_CALL(personal_data_manager, RecordUseOf(GuidMatches(address.guid())))
      .Times(1);
  EXPECT_CALL(personal_data_manager,
              RecordUseOf(GuidMatches(credit_card.guid())))
      .Times(1);

  payment_request.RecordUseStats();
}

// Test that the use counts of the data models are updated as expected when no
// contact information is requested.
TEST_F(PaymentRequestTest, RecordUseStats_RequestShippingOnly) {
  MockTestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  personal_data_manager.AddTestingProfile(&address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  personal_data_manager.AddTestingCreditCard(&credit_card);
  credit_card.set_billing_address_id(address.guid());

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();
  web_payment_request.options.request_payer_name = false;
  web_payment_request.options.request_payer_email = false;
  web_payment_request.options.request_payer_phone = false;

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentInstrument* payment_instrument =
      static_cast<AutofillPaymentInstrument*>(
          payment_request.selected_payment_method());
  EXPECT_EQ(address.guid(),
            payment_request.selected_shipping_profile()->guid());
  EXPECT_EQ(nullptr, payment_request.selected_contact_profile());
  EXPECT_EQ(credit_card.guid(), payment_instrument->credit_card()->guid());

  EXPECT_CALL(personal_data_manager, RecordUseOf(GuidMatches(address.guid())))
      .Times(1);
  EXPECT_CALL(personal_data_manager,
              RecordUseOf(GuidMatches(credit_card.guid())))
      .Times(1);

  payment_request.RecordUseStats();
}

// Test that the use counts of the data models are updated as expected when no
// shipping information is requested.
TEST_F(PaymentRequestTest, RecordUseStats_RequestContactInfoOnly) {
  MockTestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  personal_data_manager.AddTestingProfile(&address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  personal_data_manager.AddTestingCreditCard(&credit_card);
  credit_card.set_billing_address_id(address.guid());

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();
  web_payment_request.options.request_shipping = false;

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentInstrument* payment_instrument =
      static_cast<AutofillPaymentInstrument*>(
          payment_request.selected_payment_method());
  EXPECT_EQ(nullptr, payment_request.selected_shipping_profile());
  EXPECT_EQ(address.guid(), payment_request.selected_contact_profile()->guid());
  EXPECT_EQ(credit_card.guid(), payment_instrument->credit_card()->guid());

  EXPECT_CALL(personal_data_manager, RecordUseOf(GuidMatches(address.guid())))
      .Times(1);
  EXPECT_CALL(personal_data_manager,
              RecordUseOf(GuidMatches(credit_card.guid())))
      .Times(1);

  payment_request.RecordUseStats();
}

// Test that the use counts of the data models are updated as expected when no
// shipping or contact information is requested.
TEST_F(PaymentRequestTest, RecordUseStats_NoShippingOrContactInfoRequested) {
  MockTestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  personal_data_manager.AddTestingProfile(&address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  personal_data_manager.AddTestingCreditCard(&credit_card);
  credit_card.set_billing_address_id(address.guid());

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();
  web_payment_request.options.request_shipping = false;
  web_payment_request.options.request_payer_name = false;
  web_payment_request.options.request_payer_email = false;
  web_payment_request.options.request_payer_phone = false;

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentInstrument* payment_instrument =
      static_cast<AutofillPaymentInstrument*>(
          payment_request.selected_payment_method());
  EXPECT_EQ(nullptr, payment_request.selected_shipping_profile());
  EXPECT_EQ(nullptr, payment_request.selected_contact_profile());
  EXPECT_EQ(credit_card.guid(), payment_instrument->credit_card()->guid());

  EXPECT_CALL(personal_data_manager, RecordUseOf(GuidMatches(address.guid())))
      .Times(0);
  EXPECT_CALL(personal_data_manager,
              RecordUseOf(GuidMatches(credit_card.guid())))
      .Times(1);

  payment_request.RecordUseStats();
}

// Tests that the modifier should not get applied when the card network is not
// supported.
TEST_F(PaymentRequestTest, PaymentDetailsModifier_BasicCard_NetworkMismatch) {
  autofill::TestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  personal_data_manager.AddTestingProfile(&address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();  // Visa.
  personal_data_manager.AddTestingCreditCard(&credit_card);
  credit_card.set_billing_address_id(address.guid());

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();
  PaymentDetailsModifier modifier;
  modifier.method_data.supported_methods.push_back("basic-card");
  modifier.method_data.supported_networks.push_back("amex");
  modifier.total = base::MakeUnique<payments::PaymentItem>();
  modifier.total->label = "Discounted Total";
  modifier.total->amount.value = "0.99";
  modifier.total->amount.currency = "USD";
  payments::PaymentItem additional_display_item;
  additional_display_item.label = "Amex discount";
  additional_display_item.amount.value = "-0.01";
  additional_display_item.amount.currency = "USD";
  modifier.additional_display_items.push_back(additional_display_item);
  web_payment_request.details.modifiers.push_back(modifier);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentInstrument* selected_payment_method =
      static_cast<AutofillPaymentInstrument*>(
          payment_request.selected_payment_method());
  EXPECT_EQ("Total", payment_request.GetTotal(selected_payment_method).label);
  EXPECT_EQ("1.00",
            payment_request.GetTotal(selected_payment_method).amount.value);
  ASSERT_EQ(1U,
            payment_request.GetDisplayItems(selected_payment_method).size());
}

// Tests that the modifier should get applied when the card network is a match.
TEST_F(PaymentRequestTest, PaymentDetailsModifier_BasicCard_NetworkMatch) {
  autofill::TestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  personal_data_manager.AddTestingProfile(&address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard2();  // Amex.
  personal_data_manager.AddTestingCreditCard(&credit_card);
  credit_card.set_billing_address_id(address.guid());

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();
  PaymentDetailsModifier modifier;
  modifier.method_data.supported_methods.push_back("basic-card");
  modifier.method_data.supported_networks.push_back("amex");
  modifier.total = base::MakeUnique<payments::PaymentItem>();
  modifier.total->label = "Discounted Total";
  modifier.total->amount.value = "0.99";
  modifier.total->amount.currency = "USD";
  payments::PaymentItem additional_display_item;
  additional_display_item.label = "Amex discount";
  additional_display_item.amount.value = "-0.01";
  additional_display_item.amount.currency = "USD";
  modifier.additional_display_items.push_back(additional_display_item);
  web_payment_request.details.modifiers.push_back(modifier);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentInstrument* selected_payment_method =
      static_cast<AutofillPaymentInstrument*>(
          payment_request.selected_payment_method());
  EXPECT_EQ("Discounted Total",
            payment_request.GetTotal(selected_payment_method).label);
  EXPECT_EQ("0.99",
            payment_request.GetTotal(selected_payment_method).amount.value);
  ASSERT_EQ(2U,
            payment_request.GetDisplayItems(selected_payment_method).size());
  EXPECT_EQ("Subtotal",
            payment_request.GetDisplayItems(selected_payment_method)[0].label);
  EXPECT_EQ(
      "1.00",
      payment_request.GetDisplayItems(selected_payment_method)[0].amount.value);
  EXPECT_EQ("Amex discount",
            payment_request.GetDisplayItems(selected_payment_method)[1].label);
  EXPECT_EQ(
      "-0.01",
      payment_request.GetDisplayItems(selected_payment_method)[1].amount.value);
}

// Tests that the modifier should not get applied when the card type is not
// supported.
TEST_F(PaymentRequestTest, PaymentDetailsModifier_BasicCard_TypeMismatch) {
  autofill::TestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  personal_data_manager.AddTestingProfile(&address);
  autofill::CreditCard credit_card = autofill::test::GetCreditCard2();  // Amex.
  personal_data_manager.AddTestingCreditCard(&credit_card);
  credit_card.set_billing_address_id(address.guid());

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();
  PaymentDetailsModifier modifier;
  modifier.method_data.supported_methods.push_back("basic-card");
  modifier.method_data.supported_networks.push_back("amex");
  modifier.method_data.supported_types.insert(
      autofill::CreditCard::CARD_TYPE_CREDIT);
  modifier.total = base::MakeUnique<payments::PaymentItem>();
  modifier.total->label = "Discounted Total";
  modifier.total->amount.value = "0.99";
  modifier.total->amount.currency = "USD";
  payments::PaymentItem additional_display_item;
  additional_display_item.label = "Amex discount";
  additional_display_item.amount.value = "-0.01";
  additional_display_item.amount.currency = "USD";
  modifier.additional_display_items.push_back(additional_display_item);
  web_payment_request.details.modifiers.push_back(modifier);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentInstrument* selected_payment_method =
      static_cast<AutofillPaymentInstrument*>(
          payment_request.selected_payment_method());
  EXPECT_EQ("Total", payment_request.GetTotal(selected_payment_method).label);
  EXPECT_EQ("1.00",
            payment_request.GetTotal(selected_payment_method).amount.value);
  ASSERT_EQ(1U,
            payment_request.GetDisplayItems(selected_payment_method).size());
}

// Tests that the modifier should get applied when the card network and the type
// are both a match.
TEST_F(PaymentRequestTest,
       PaymentDetailsModifier_BasicCard_NetworkAndTypeMatch) {
  autofill::TestPersonalDataManager personal_data_manager;
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  personal_data_manager.AddTestingProfile(&address);
  autofill::CreditCard credit_card = autofill::test::GetMaskedServerCardAmex();
  credit_card.set_card_type(autofill::CreditCard::CardType::CARD_TYPE_CREDIT);
  personal_data_manager.AddTestingCreditCard(&credit_card);
  credit_card.set_billing_address_id(address.guid());

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();
  PaymentDetailsModifier modifier;
  modifier.method_data.supported_methods.push_back("basic-card");
  modifier.method_data.supported_networks.push_back("amex");
  modifier.method_data.supported_types.insert(
      autofill::CreditCard::CARD_TYPE_CREDIT);
  modifier.total = base::MakeUnique<payments::PaymentItem>();
  modifier.total->label = "Discounted Total";
  modifier.total->amount.value = "0.99";
  modifier.total->amount.currency = "USD";
  payments::PaymentItem additional_display_item;
  additional_display_item.label = "Amex discount";
  additional_display_item.amount.value = "-0.01";
  additional_display_item.amount.currency = "USD";
  modifier.additional_display_items.push_back(additional_display_item);
  web_payment_request.details.modifiers.push_back(modifier);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &personal_data_manager);
  AutofillPaymentInstrument* selected_payment_method =
      static_cast<AutofillPaymentInstrument*>(
          payment_request.selected_payment_method());
  EXPECT_EQ("Discounted Total",
            payment_request.GetTotal(selected_payment_method).label);
  EXPECT_EQ("0.99",
            payment_request.GetTotal(selected_payment_method).amount.value);
  ASSERT_EQ(2U,
            payment_request.GetDisplayItems(selected_payment_method).size());
  EXPECT_EQ("Subtotal",
            payment_request.GetDisplayItems(selected_payment_method)[0].label);
  EXPECT_EQ(
      "1.00",
      payment_request.GetDisplayItems(selected_payment_method)[0].amount.value);
  EXPECT_EQ("Amex discount",
            payment_request.GetDisplayItems(selected_payment_method)[1].label);
  EXPECT_EQ(
      "-0.01",
      payment_request.GetDisplayItems(selected_payment_method)[1].amount.value);
}

}  // namespace payments
