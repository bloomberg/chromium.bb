// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/payment_request.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/core/currency_formatter.h"
#include "components/payments/core/payment_method_data.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests that the payments::CurrencyFormatter is constructed with the correct
// currency code and currency system.
TEST(PaymentRequestTest, CreatesCurrencyFormatterCorrectly) {
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
TEST(PaymentRequestTest, AcceptedPaymentNetworks) {
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
TEST(PaymentRequestTest, SupportedMethods) {
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
TEST(PaymentRequestTest, SupportedMethods_MultipleEntries) {
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
TEST(PaymentRequestTest, SupportedMethods_OnlyBasicCard) {
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
TEST(PaymentRequestTest, SupportedMethods_BasicCard_WithSpecificMethod) {
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
TEST(PaymentRequestTest, SupportedMethods_BasicCard_Overlap) {
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
TEST(PaymentRequestTest, SupportedMethods_BasicCard_WithSupportedNetworks) {
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
TEST(PaymentRequestTest, AddCreditCard) {
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
