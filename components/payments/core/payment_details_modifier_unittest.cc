// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_details_modifier.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests that serializing a default PaymentDetailsModifier yields the expected
// result.
TEST(PaymentRequestTest, EmptyPaymentDetailsModifierDictionary) {
  base::DictionaryValue expected_value;

  std::unique_ptr<base::ListValue> supported_methods_list =
      base::MakeUnique<base::ListValue>();
  expected_value.SetList("supportedMethods", std::move(supported_methods_list));
  std::unique_ptr<base::DictionaryValue> item_dict =
      base::MakeUnique<base::DictionaryValue>();
  item_dict->SetString("label", "");
  std::unique_ptr<base::DictionaryValue> amount_dict =
      base::MakeUnique<base::DictionaryValue>();
  amount_dict->SetString("currency", "");
  amount_dict->SetString("value", "");
  amount_dict->SetString("currencySystem", "urn:iso:std:iso:4217");
  item_dict->SetDictionary("amount", std::move(amount_dict));
  item_dict->SetBoolean("pending", false);
  expected_value.SetDictionary("total", std::move(item_dict));

  PaymentDetailsModifier payment_detials_modififer;
  EXPECT_TRUE(expected_value.Equals(
      payment_detials_modififer.ToDictionaryValue().get()));
}

// Tests that serializing a populated PaymentDetailsModifier yields the expected
// result.
TEST(PaymentRequestTest, PopulatedDetailsModifierDictionary) {
  base::DictionaryValue expected_value;

  std::unique_ptr<base::ListValue> supported_methods_list =
      base::MakeUnique<base::ListValue>();
  supported_methods_list->GetList().emplace_back("visa");
  supported_methods_list->GetList().emplace_back("amex");
  expected_value.SetList("supportedMethods", std::move(supported_methods_list));
  std::unique_ptr<base::DictionaryValue> item_dict =
      base::MakeUnique<base::DictionaryValue>();
  item_dict->SetString("label", "Gratuity");
  std::unique_ptr<base::DictionaryValue> amount_dict =
      base::MakeUnique<base::DictionaryValue>();
  amount_dict->SetString("currency", "USD");
  amount_dict->SetString("value", "139.99");
  amount_dict->SetString("currencySystem", "urn:iso:std:iso:4217");
  item_dict->SetDictionary("amount", std::move(amount_dict));
  item_dict->SetBoolean("pending", false);
  expected_value.SetDictionary("total", std::move(item_dict));

  PaymentDetailsModifier payment_detials_modififer;
  payment_detials_modififer.supported_methods.push_back(
      base::ASCIIToUTF16("visa"));
  payment_detials_modififer.supported_methods.push_back(
      base::ASCIIToUTF16("amex"));
  payment_detials_modififer.total.label = base::ASCIIToUTF16("Gratuity");
  payment_detials_modififer.total.amount.currency = base::ASCIIToUTF16("USD");
  payment_detials_modififer.total.amount.value = base::ASCIIToUTF16("139.99");

  EXPECT_TRUE(expected_value.Equals(
      payment_detials_modififer.ToDictionaryValue().get()));
}

// Tests that two details modifier objects are not equal if their property
// values differ or one is missing a value present in the other, and equal
// otherwise. Doesn't test all properties of child objects, relying instead on
// their respective tests.
TEST(PaymentRequestTest, PaymentDetailsModifierEquality) {
  PaymentDetailsModifier details_modifier1;
  PaymentDetailsModifier details_modifier2;
  EXPECT_EQ(details_modifier1, details_modifier2);

  std::vector<base::string16> supported_methods1;
  supported_methods1.push_back(base::ASCIIToUTF16("China UnionPay"));
  supported_methods1.push_back(base::ASCIIToUTF16("BobPay"));
  details_modifier1.supported_methods = supported_methods1;
  EXPECT_NE(details_modifier1, details_modifier2);
  std::vector<base::string16> supported_methods2;
  supported_methods2.push_back(base::ASCIIToUTF16("BobPay"));
  details_modifier2.supported_methods = supported_methods2;
  EXPECT_NE(details_modifier1, details_modifier2);
  details_modifier2.supported_methods = supported_methods1;
  EXPECT_EQ(details_modifier1, details_modifier2);

  details_modifier1.total.label = base::ASCIIToUTF16("Total");
  EXPECT_NE(details_modifier1, details_modifier2);
  details_modifier2.total.label = base::ASCIIToUTF16("Gratuity");
  EXPECT_NE(details_modifier1, details_modifier2);
  details_modifier2.total.label = base::ASCIIToUTF16("Total");
  EXPECT_EQ(details_modifier1, details_modifier2);

  PaymentItem payment_item;
  payment_item.label = base::ASCIIToUTF16("Tax");
  std::vector<PaymentItem> display_items1;
  display_items1.push_back(payment_item);
  details_modifier1.additional_display_items = display_items1;
  EXPECT_NE(details_modifier1, details_modifier2);
  std::vector<PaymentItem> display_items2;
  display_items2.push_back(payment_item);
  display_items2.push_back(payment_item);
  details_modifier2.additional_display_items = display_items2;
  EXPECT_NE(details_modifier1, details_modifier2);
  details_modifier2.additional_display_items = display_items1;
  EXPECT_EQ(details_modifier1, details_modifier2);
}

}  // namespace payments
