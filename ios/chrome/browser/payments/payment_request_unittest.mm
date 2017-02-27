// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/payment_request.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/core/currency_formatter.h"
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

  autofill::TestPersonalDataManager personal_data_manager;

  std::unique_ptr<web::PaymentRequest> web_payment_request =
      base::MakeUnique<web::PaymentRequest>();
  web_payment_request->details.total.amount.currency =
      base::ASCIIToUTF16("USD");
  PaymentRequest payment_request1(std::move(web_payment_request),
                                  &personal_data_manager);
  payments::CurrencyFormatter* currency_formatter =
      payment_request1.GetOrCreateCurrencyFormatter();
  ASSERT_EQ(base::UTF8ToUTF16("$55.00"), currency_formatter->Format("55.00"));
  ASSERT_EQ("USD", currency_formatter->formatted_currency_code());

  web_payment_request = base::MakeUnique<web::PaymentRequest>();
  web_payment_request->details.total.amount.currency =
      base::ASCIIToUTF16("JPY");
  PaymentRequest payment_request2(std::move(web_payment_request),
                                  &personal_data_manager);
  currency_formatter = payment_request2.GetOrCreateCurrencyFormatter();
  ASSERT_EQ(base::UTF8ToUTF16("¥55"), currency_formatter->Format("55.00"));
  ASSERT_EQ("JPY", currency_formatter->formatted_currency_code());

  web_payment_request = base::MakeUnique<web::PaymentRequest>();
  web_payment_request->details.total.amount.currency_system =
      base::ASCIIToUTF16("NOT_ISO4217");
  web_payment_request->details.total.amount.currency =
      base::ASCIIToUTF16("USD");
  PaymentRequest payment_request3(std::move(web_payment_request),
                                  &personal_data_manager);
  currency_formatter = payment_request3.GetOrCreateCurrencyFormatter();
  ASSERT_EQ(base::UTF8ToUTF16("55.00"), currency_formatter->Format("55.00"));
  ASSERT_EQ("USD", currency_formatter->formatted_currency_code());
}
