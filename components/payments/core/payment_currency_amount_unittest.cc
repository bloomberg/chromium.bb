// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_currency_amount.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests the success case when populating a PaymentCurrencyAmount from a
// dictionary.
TEST(PaymentRequestTest, PaymentCurrencyAmountFromDictionaryValueSuccess) {
  PaymentCurrencyAmount expected;
  expected.currency = "AUD";
  expected.value = "-438.23";

  base::DictionaryValue amount_dict;
  amount_dict.SetString("currency", "AUD");
  amount_dict.SetString("value", "-438.23");

  PaymentCurrencyAmount actual;
  EXPECT_TRUE(actual.FromDictionaryValue(amount_dict));

  EXPECT_EQ(expected, actual);

  expected.currency_system = "urn:iso:std:iso:123456789";
  amount_dict.SetString("currencySystem", "urn:iso:std:iso:123456789");
  EXPECT_TRUE(actual.FromDictionaryValue(amount_dict));
  EXPECT_EQ(expected, actual);
}

// Tests the failure case when populating a PaymentCurrencyAmount from a
// dictionary.
TEST(PaymentRequestTest, PaymentCurrencyAmountFromDictionaryValueFailure) {
  // Both a currency and a value are required.
  PaymentCurrencyAmount actual;
  base::DictionaryValue amount_dict;
  EXPECT_FALSE(actual.FromDictionaryValue(amount_dict));

  // Both values must be strings.
  amount_dict.SetInteger("currency", 842);
  amount_dict.SetString("value", "-438.23");
  EXPECT_FALSE(actual.FromDictionaryValue(amount_dict));

  amount_dict.SetString("currency", "NZD");
  amount_dict.SetDouble("value", -438.23);
  EXPECT_FALSE(actual.FromDictionaryValue(amount_dict));
}

// Tests that two currency amount objects are not equal if their property values
// differ or one is missing a value present in the other, and equal otherwise.
TEST(PaymentRequestTest, PaymentCurrencyAmountEquality) {
  PaymentCurrencyAmount currency_amount1;
  PaymentCurrencyAmount currency_amount2;
  EXPECT_EQ(currency_amount1, currency_amount2);

  currency_amount1.currency = "HKD";
  EXPECT_NE(currency_amount1, currency_amount2);
  currency_amount2.currency = "USD";
  EXPECT_NE(currency_amount1, currency_amount2);
  currency_amount2.currency = "HKD";
  EXPECT_EQ(currency_amount1, currency_amount2);

  currency_amount1.value = "49.89";
  EXPECT_NE(currency_amount1, currency_amount2);
  currency_amount2.value = "49.99";
  EXPECT_NE(currency_amount1, currency_amount2);
  currency_amount2.value = "49.89";
  EXPECT_EQ(currency_amount1, currency_amount2);
}

// Tests that serializing a default PaymentCurrencyAmount yields the expected
// result.
TEST(PaymentRequestTest, EmptyPaymentCurrencyAmountDictionary) {
  base::DictionaryValue expected_value;

  expected_value.SetString("currency", "");
  expected_value.SetString("value", "");
  expected_value.SetString("currencySystem", "urn:iso:std:iso:4217");

  PaymentCurrencyAmount payment_currency_amount;
  EXPECT_TRUE(
      expected_value.Equals(payment_currency_amount.ToDictionaryValue().get()));
}

// Tests that serializing a populated PaymentCurrencyAmount yields the expected
// result.
TEST(PaymentRequestTest, PopulatedCurrencyAmountDictionary) {
  base::DictionaryValue expected_value;

  expected_value.SetString("currency", "AUD");
  expected_value.SetString("value", "-438.23");
  expected_value.SetString("currencySystem", "urn:iso:std:iso:123456789");

  PaymentCurrencyAmount payment_currency_amount;
  payment_currency_amount.currency = "AUD";
  payment_currency_amount.value = "-438.23";
  payment_currency_amount.currency_system = "urn:iso:std:iso:123456789";

  EXPECT_TRUE(
      expected_value.Equals(payment_currency_amount.ToDictionaryValue().get()));
}

}  // namespace payments
