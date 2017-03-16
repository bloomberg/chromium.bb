// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/payment_request.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/core/currency_formatter.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
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
  PaymentRequest payment_request1(
      base::MakeUnique<web::PaymentRequest>(web_payment_request),
      &personal_data_manager);
  payments::CurrencyFormatter* currency_formatter =
      payment_request1.GetOrCreateCurrencyFormatter();
  EXPECT_EQ(base::UTF8ToUTF16("$55.00"), currency_formatter->Format("55.00"));
  EXPECT_EQ("USD", currency_formatter->formatted_currency_code());

  web_payment_request.details.total.amount.currency = base::ASCIIToUTF16("JPY");
  PaymentRequest payment_request2(
      base::MakeUnique<web::PaymentRequest>(web_payment_request),
      &personal_data_manager);
  currency_formatter = payment_request2.GetOrCreateCurrencyFormatter();
  EXPECT_EQ(base::UTF8ToUTF16("¥55"), currency_formatter->Format("55.00"));
  EXPECT_EQ("JPY", currency_formatter->formatted_currency_code());

  web_payment_request.details.total.amount.currency_system =
      base::ASCIIToUTF16("NOT_ISO4217");
  web_payment_request.details.total.amount.currency = base::ASCIIToUTF16("USD");
  PaymentRequest payment_request3(
      base::MakeUnique<web::PaymentRequest>(web_payment_request),
      &personal_data_manager);
  currency_formatter = payment_request3.GetOrCreateCurrencyFormatter();
  EXPECT_EQ(base::UTF8ToUTF16("55.00"), currency_formatter->Format("55.00"));
  EXPECT_EQ("USD", currency_formatter->formatted_currency_code());
}

// Tests that the accepted card networks are identified correctly.
TEST(PaymentRequestTest, AcceptedPaymentNetworks) {
  web::PaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  web::PaymentMethodData method_datum1;
  method_datum1.supported_methods.push_back(base::ASCIIToUTF16("visa"));
  web_payment_request.method_data.push_back(method_datum1);
  web::PaymentMethodData method_datum2;
  method_datum2.supported_methods.push_back(base::ASCIIToUTF16("mastercard"));
  web_payment_request.method_data.push_back(method_datum2);

  PaymentRequest payment_request(
      base::MakeUnique<web::PaymentRequest>(web_payment_request),
      &personal_data_manager);
  ASSERT_EQ(2U, payment_request.supported_card_networks().size());
  EXPECT_EQ("visa", payment_request.supported_card_networks()[0]);
  EXPECT_EQ("mastercard", payment_request.supported_card_networks()[1]);
}

// Tests that credit cards can be added to the list of cached credit cards.
TEST(PaymentRequestTest, AddCreditCard) {
  web::PaymentRequest web_payment_request;
  autofill::TestPersonalDataManager personal_data_manager;

  PaymentRequest payment_request(
      base::MakeUnique<web::PaymentRequest>(web_payment_request),
      &personal_data_manager);
  EXPECT_EQ(0U, payment_request.credit_cards().size());

  std::unique_ptr<autofill::CreditCard> credit_card =
      payment_request_test_util::CreateTestCreditCard();
  const autofill::CreditCard* added_credit_card = credit_card.get();
  payment_request.AddCreditCard(std::move(credit_card));
  ASSERT_EQ(1U, payment_request.credit_cards().size());
  EXPECT_EQ(added_credit_card, payment_request.credit_cards()[0]);
}
