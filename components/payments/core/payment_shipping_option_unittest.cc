// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_shipping_option.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests the success case when populating a PaymentShippingOption from a
// dictionary.
TEST(PaymentRequestTest, PaymentShippingOptionFromDictionaryValueSuccess) {
  PaymentShippingOption expected;
  expected.id = base::ASCIIToUTF16("123");
  expected.label = base::ASCIIToUTF16("Ground Shipping");
  expected.amount.currency = base::ASCIIToUTF16("BRL");
  expected.amount.value = base::ASCIIToUTF16("4,000.32");
  expected.selected = true;

  base::DictionaryValue shipping_option_dict;
  shipping_option_dict.SetString("id", "123");
  shipping_option_dict.SetString("label", "Ground Shipping");
  std::unique_ptr<base::DictionaryValue> amount_dict(new base::DictionaryValue);
  amount_dict->SetString("currency", "BRL");
  amount_dict->SetString("value", "4,000.32");
  shipping_option_dict.Set("amount", std::move(amount_dict));
  shipping_option_dict.SetBoolean("selected", true);

  PaymentShippingOption actual;
  EXPECT_TRUE(actual.FromDictionaryValue(shipping_option_dict));

  EXPECT_EQ(expected, actual);
}

// Tests the failure case when populating a PaymentShippingOption from a
// dictionary.
TEST(PaymentRequestTest, PaymentShippingOptionFromDictionaryValueFailure) {
  PaymentShippingOption expected;
  expected.id = base::ASCIIToUTF16("123");
  expected.label = base::ASCIIToUTF16("Ground Shipping");
  expected.amount.currency = base::ASCIIToUTF16("BRL");
  expected.amount.value = base::ASCIIToUTF16("4,000.32");
  expected.selected = true;

  PaymentShippingOption actual;
  base::DictionaryValue shipping_option_dict;

  // Id, Label, and amount are required.
  shipping_option_dict.SetString("id", "123");
  EXPECT_FALSE(actual.FromDictionaryValue(shipping_option_dict));

  shipping_option_dict.SetString("label", "Ground Shipping");
  EXPECT_FALSE(actual.FromDictionaryValue(shipping_option_dict));

  // Id must be a string.
  std::unique_ptr<base::DictionaryValue> amount_dict(new base::DictionaryValue);
  amount_dict->SetString("currency", "BRL");
  amount_dict->SetString("value", "4,000.32");
  shipping_option_dict.Set("amount", std::move(amount_dict));
  shipping_option_dict.SetInteger("id", 123);
  EXPECT_FALSE(actual.FromDictionaryValue(shipping_option_dict));

  // Label must be a string.
  shipping_option_dict.SetString("id", "123");
  shipping_option_dict.SetInteger("label", 123);
  EXPECT_FALSE(actual.FromDictionaryValue(shipping_option_dict));
}

// Tests that two shipping option objects are not equal if their property values
// differ or one is missing a value present in the other, and equal otherwise.
// Doesn't test all properties of child objects, relying instead on their
// respective tests.
TEST(PaymentRequestTest, PaymentShippingOptionEquality) {
  PaymentShippingOption shipping_option1;
  PaymentShippingOption shipping_option2;
  EXPECT_EQ(shipping_option1, shipping_option2);

  shipping_option1.id = base::ASCIIToUTF16("a8df2");
  EXPECT_NE(shipping_option1, shipping_option2);
  shipping_option2.id = base::ASCIIToUTF16("k42jk");
  EXPECT_NE(shipping_option1, shipping_option2);
  shipping_option2.id = base::ASCIIToUTF16("a8df2");
  EXPECT_EQ(shipping_option1, shipping_option2);

  shipping_option1.label = base::ASCIIToUTF16("Overnight");
  EXPECT_NE(shipping_option1, shipping_option2);
  shipping_option2.label = base::ASCIIToUTF16("Ground");
  EXPECT_NE(shipping_option1, shipping_option2);
  shipping_option2.label = base::ASCIIToUTF16("Overnight");
  EXPECT_EQ(shipping_option1, shipping_option2);

  shipping_option1.amount.currency = base::ASCIIToUTF16("AUD");
  EXPECT_NE(shipping_option1, shipping_option2);
  shipping_option2.amount.currency = base::ASCIIToUTF16("HKD");
  EXPECT_NE(shipping_option1, shipping_option2);
  shipping_option2.amount.currency = base::ASCIIToUTF16("AUD");
  EXPECT_EQ(shipping_option1, shipping_option2);

  shipping_option1.selected = true;
  EXPECT_NE(shipping_option1, shipping_option2);
  shipping_option2.selected = true;
  EXPECT_EQ(shipping_option1, shipping_option2);
}

}  // namespace payments
