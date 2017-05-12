// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/payment_request.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/core/currency_formatter.h"
#include "components/payments/core/payment_method_data.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PaymentRequestTest : public testing::Test {
 protected:
  // Returns PaymentDetails with one shipping option that's selected.
  web::PaymentDetails CreateDetailsWithShippingOption() {
    web::PaymentDetails details;
    std::vector<web::PaymentShippingOption> shipping_options;
    web::PaymentShippingOption option1;
    option1.id = base::UTF8ToUTF16("option:1");
    option1.selected = true;
    shipping_options.push_back(std::move(option1));
    details.shipping_options = std::move(shipping_options);

    return details;
  }

  web::PaymentOptions CreatePaymentOptions(bool request_payer_name,
                                           bool request_payer_phone,
                                           bool request_payer_email,
                                           bool request_shipping) {
    web::PaymentOptions options;
    options.request_payer_name = request_payer_name;
    options.request_payer_phone = request_payer_phone;
    options.request_payer_email = request_payer_email;
    options.request_shipping = request_shipping;
    return options;
  }
};

// Tests that the payments::CurrencyFormatter is constructed with the correct
// currency code and currency system.
TEST_F(PaymentRequestTest, CreatesCurrencyFormatterCorrectly) {
  ASSERT_EQ("en", GetApplicationContext()->GetApplicationLocale());

  web::PaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  web_payment_request.details.total.amount.currency = base::ASCIIToUTF16("USD");
  PaymentRequest payment_request1(web_payment_request, &personal_data_manager);
  payments::CurrencyFormatter* currency_formatter =
      payment_request1.GetOrCreateCurrencyFormatter();
  EXPECT_EQ(base::UTF8ToUTF16("$55.00"), currency_formatter->Format("55.00"));
  EXPECT_EQ("USD", currency_formatter->formatted_currency_code());

  web_payment_request.details.total.amount.currency = base::ASCIIToUTF16("JPY");
  PaymentRequest payment_request2(web_payment_request, &personal_data_manager);
  currency_formatter = payment_request2.GetOrCreateCurrencyFormatter();
  EXPECT_EQ(base::UTF8ToUTF16("¥55"), currency_formatter->Format("55.00"));
  EXPECT_EQ("JPY", currency_formatter->formatted_currency_code());

  web_payment_request.details.total.amount.currency_system =
      base::ASCIIToUTF16("NOT_ISO4217");
  web_payment_request.details.total.amount.currency = base::ASCIIToUTF16("USD");
  PaymentRequest payment_request3(web_payment_request, &personal_data_manager);
  currency_formatter = payment_request3.GetOrCreateCurrencyFormatter();
  EXPECT_EQ(base::UTF8ToUTF16("55.00"), currency_formatter->Format("55.00"));
  EXPECT_EQ("USD", currency_formatter->formatted_currency_code());
}

// Tests that the accepted card networks are identified correctly.
TEST_F(PaymentRequestTest, AcceptedPaymentNetworks) {
  web::PaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  payments::PaymentMethodData method_datum1;
  method_datum1.supported_methods.push_back("visa");
  web_payment_request.method_data.push_back(method_datum1);
  payments::PaymentMethodData method_datum2;
  method_datum2.supported_methods.push_back("mastercard");
  web_payment_request.method_data.push_back(method_datum2);

  PaymentRequest payment_request(web_payment_request, &personal_data_manager);
  ASSERT_EQ(2U, payment_request.supported_card_networks().size());
  EXPECT_EQ("visa", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("mastercard", payment_request.supported_card_networks()[1]);
}

// Test that parsing supported methods (with invalid values and duplicates)
// works as expected.
TEST_F(PaymentRequestTest, SupportedMethods) {
  web::PaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  payments::PaymentMethodData method_datum1;
  method_datum1.supported_methods.push_back("visa");
  method_datum1.supported_methods.push_back("mastercard");
  method_datum1.supported_methods.push_back("invalid");
  method_datum1.supported_methods.push_back("");
  method_datum1.supported_methods.push_back("visa");
  web_payment_request.method_data.push_back(method_datum1);

  PaymentRequest payment_request(web_payment_request, &personal_data_manager);
  ASSERT_EQ(2U, payment_request.supported_card_networks().size());
  EXPECT_EQ("visa", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("mastercard", payment_request.supported_card_networks()[1]);
}

// Test that parsing supported methods in different method data entries (with
// invalid values and duplicates) works as expected.
TEST_F(PaymentRequestTest, SupportedMethods_MultipleEntries) {
  web::PaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  payments::PaymentMethodData method_datum1;
  method_datum1.supported_methods.push_back("visa");
  web_payment_request.method_data.push_back(method_datum1);
  payments::PaymentMethodData method_datum2;
  method_datum2.supported_methods.push_back("mastercard");
  web_payment_request.method_data.push_back(method_datum2);
  payments::PaymentMethodData method_datum3;
  method_datum3.supported_methods.push_back("");
  web_payment_request.method_data.push_back(method_datum3);
  payments::PaymentMethodData method_datum4;
  method_datum4.supported_methods.push_back("visa");
  web_payment_request.method_data.push_back(method_datum4);

  PaymentRequest payment_request(web_payment_request, &personal_data_manager);
  ASSERT_EQ(2U, payment_request.supported_card_networks().size());
  EXPECT_EQ("visa", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("mastercard", payment_request.supported_card_networks()[1]);
}

// Test that only specifying basic-card means that all are supported.
TEST_F(PaymentRequestTest, SupportedMethods_OnlyBasicCard) {
  web::PaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  payments::PaymentMethodData method_datum1;
  method_datum1.supported_methods.push_back("basic-card");
  web_payment_request.method_data.push_back(method_datum1);

  PaymentRequest payment_request(web_payment_request, &personal_data_manager);

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
}

// Test that specifying a method AND basic-card means that all are supported,
// but with the method as first.
TEST_F(PaymentRequestTest, SupportedMethods_BasicCard_WithSpecificMethod) {
  web::PaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  payments::PaymentMethodData method_datum1;
  method_datum1.supported_methods.push_back("jcb");
  method_datum1.supported_methods.push_back("basic-card");
  web_payment_request.method_data.push_back(method_datum1);

  PaymentRequest payment_request(web_payment_request, &personal_data_manager);

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
  web::PaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  payments::PaymentMethodData method_datum1;
  method_datum1.supported_methods.push_back("mastercard");
  method_datum1.supported_methods.push_back("visa");
  web_payment_request.method_data.push_back(method_datum1);
  payments::PaymentMethodData method_datum2;
  method_datum2.supported_methods.push_back("basic-card");
  method_datum2.supported_networks.push_back("visa");
  method_datum2.supported_networks.push_back("mastercard");
  method_datum2.supported_networks.push_back("unionpay");
  web_payment_request.method_data.push_back(method_datum2);

  PaymentRequest payment_request(web_payment_request, &personal_data_manager);

  EXPECT_EQ(3u, payment_request.supported_card_networks().size());
  EXPECT_EQ("mastercard", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("visa", payment_request.supported_card_networks()[1]);
  EXPECT_EQ("unionpay", payment_request.supported_card_networks()[2]);
}

// Test that specifying basic-card with supported networks after specifying
// some methods
TEST_F(PaymentRequestTest, SupportedMethods_BasicCard_WithSupportedNetworks) {
  web::PaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  payments::PaymentMethodData method_datum1;
  method_datum1.supported_methods.push_back("basic-card");
  method_datum1.supported_networks.push_back("visa");
  method_datum1.supported_networks.push_back("unionpay");
  web_payment_request.method_data.push_back(method_datum1);

  PaymentRequest payment_request(web_payment_request, &personal_data_manager);

  // Only the specified networks are supported.
  EXPECT_EQ(2u, payment_request.supported_card_networks().size());
  EXPECT_EQ("visa", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("unionpay", payment_request.supported_card_networks()[1]);
}

// Tests that credit cards can be added to the list of cached credit cards.
TEST_F(PaymentRequestTest, AddCreditCard) {
  web::PaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  PaymentRequest payment_request(web_payment_request, &personal_data_manager);
  EXPECT_EQ(0U, payment_request.credit_cards().size());

  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  autofill::CreditCard* added_credit_card =
      payment_request.AddCreditCard(credit_card);

  ASSERT_EQ(1U, payment_request.credit_cards().size());
  EXPECT_EQ(credit_card, *added_credit_card);
}

// Test that parsing shipping options works as expected.
TEST_F(PaymentRequestTest, SelectedShippingOptions) {
  web::PaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  web::PaymentDetails details;
  std::vector<web::PaymentShippingOption> shipping_options;
  web::PaymentShippingOption option1;
  option1.id = base::UTF8ToUTF16("option:1");
  option1.selected = false;
  shipping_options.push_back(std::move(option1));
  web::PaymentShippingOption option2;
  option2.id = base::UTF8ToUTF16("option:2");
  option2.selected = true;
  shipping_options.push_back(std::move(option2));
  web::PaymentShippingOption option3;
  option3.id = base::UTF8ToUTF16("option:3");
  option3.selected = true;
  shipping_options.push_back(std::move(option3));
  details.shipping_options = std::move(shipping_options);
  web_payment_request.details = std::move(details);

  PaymentRequest payment_request(web_payment_request, &personal_data_manager);
  // The last one marked "selected" should be selected.
  EXPECT_EQ(base::UTF8ToUTF16("option:3"),
            payment_request.selected_shipping_option()->id);

  // Simulate an update that no longer has any shipping options. There is no
  // longer a selected shipping option.
  web::PaymentDetails new_details;
  payment_request.UpdatePaymentDetails(std::move(new_details));
  EXPECT_EQ(nullptr, payment_request.selected_shipping_option());
}

// Test that loading profiles when none are available works as expected.
TEST_F(PaymentRequestTest, SelectedProfiles_NoProfiles) {
  autofill::TestPersonalDataManager personal_data_manager;
  web::PaymentRequest web_payment_request;
  web_payment_request.details = CreateDetailsWithShippingOption();
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  // No profiles are selected because none are available!
  PaymentRequest payment_request(web_payment_request, &personal_data_manager);
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

  web::PaymentRequest web_payment_request;
  web_payment_request.details = CreateDetailsWithShippingOption();
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  // address2 is selected because it has the most use count (Frecency model).
  PaymentRequest payment_request(web_payment_request, &personal_data_manager);
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

  web::PaymentRequest web_payment_request;
  // No shipping options.
  web_payment_request.details = web::PaymentDetails();
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  // No shipping profile is selected because the merchant has not selected a
  // shipping option. However there is a suitable contact profile.
  PaymentRequest payment_request(web_payment_request, &personal_data_manager);
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

  web::PaymentRequest web_payment_request;
  web_payment_request.details = CreateDetailsWithShippingOption();
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/true,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  // Even though address1 has more use counts, address2 is selected because it
  // is complete.
  PaymentRequest payment_request(web_payment_request, &personal_data_manager);
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

  web::PaymentRequest web_payment_request;
  web_payment_request.details = CreateDetailsWithShippingOption();
  // The merchant doesn't care about the phone number.
  web_payment_request.options = CreatePaymentOptions(
      /*request_payer_name=*/true, /*request_payer_phone=*/false,
      /*request_payer_email=*/true, /*request_shipping=*/true);

  // address1 has more use counts, and even though it has no phone number, it's
  // still selected as the contact profile because merchant doesn't require
  // phone. address2 is selected as the shipping profile because it's the most
  // complete for shipping.
  PaymentRequest payment_request(web_payment_request, &personal_data_manager);
  EXPECT_EQ(address2.guid(),
            payment_request.selected_shipping_profile()->guid());
  EXPECT_EQ(address1.guid(),
            payment_request.selected_contact_profile()->guid());
}
