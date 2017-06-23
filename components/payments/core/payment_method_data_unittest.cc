// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_method_data.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests the success case when populating a PaymentMethodData from a dictionary.
TEST(PaymentMethodData, FromDictionaryValueSuccess) {
  PaymentMethodData expected;
  expected.supported_methods.push_back("visa");
  expected.supported_methods.push_back("basic-card");
  expected.data =
      "{\"supportedNetworks\":[\"mastercard\"],"
      "\"supportedTypes\":[\"debit\",\"credit\"]}";
  expected.supported_networks.push_back("mastercard");
  expected.supported_types.insert(autofill::CreditCard::CARD_TYPE_DEBIT);
  expected.supported_types.insert(autofill::CreditCard::CARD_TYPE_CREDIT);

  base::DictionaryValue method_data_dict;
  std::unique_ptr<base::ListValue> supported_methods_list(new base::ListValue);
  supported_methods_list->AppendString("visa");
  supported_methods_list->AppendString("basic-card");
  method_data_dict.Set("supportedMethods", std::move(supported_methods_list));
  std::unique_ptr<base::DictionaryValue> data_dict(new base::DictionaryValue);
  std::unique_ptr<base::ListValue> supported_networks_list(new base::ListValue);
  supported_networks_list->AppendString("mastercard");
  data_dict->Set("supportedNetworks", std::move(supported_networks_list));
  std::unique_ptr<base::ListValue> supported_types_list(new base::ListValue);
  supported_types_list->AppendString("debit");
  supported_types_list->AppendString("credit");
  data_dict->Set("supportedTypes", std::move(supported_types_list));
  method_data_dict.Set("data", std::move(data_dict));

  PaymentMethodData actual;
  EXPECT_TRUE(actual.FromDictionaryValue(method_data_dict));

  EXPECT_EQ(expected, actual);
}

// Tests the failure case when populating a PaymentMethodData from a dictionary.
TEST(PaymentMethodData, FromDictionaryValueFailure) {
  // At least one supported method is required.
  PaymentMethodData actual;
  base::DictionaryValue method_data_dict;
  EXPECT_FALSE(actual.FromDictionaryValue(method_data_dict));

  // The value in the supported methods list must be a string.
  std::unique_ptr<base::ListValue> supported_methods_list(new base::ListValue);
  supported_methods_list->AppendInteger(13);
  method_data_dict.Set("supportedMethods", std::move(supported_methods_list));
  EXPECT_FALSE(actual.FromDictionaryValue(method_data_dict));
}

// Tests that two method data objects are not equal if their property values
// differ or one is missing a value present in the other, and equal otherwise.
TEST(PaymentMethodData, Equality) {
  PaymentMethodData method_data1;
  PaymentMethodData method_data2;
  EXPECT_EQ(method_data1, method_data2);

  std::vector<std::string> supported_methods1;
  supported_methods1.push_back("basic-card");
  supported_methods1.push_back("http://bobpay.com");
  method_data1.supported_methods = supported_methods1;
  EXPECT_NE(method_data1, method_data2);
  std::vector<std::string> supported_methods2;
  supported_methods2.push_back("http://bobpay.com");
  method_data2.supported_methods = supported_methods2;
  EXPECT_NE(method_data1, method_data2);
  method_data2.supported_methods = supported_methods1;
  EXPECT_EQ(method_data1, method_data2);

  method_data1.data = "{merchantId: '123456'}";
  EXPECT_NE(method_data1, method_data2);
  method_data2.data = "{merchantId: '9999-88'}";
  EXPECT_NE(method_data1, method_data2);
  method_data2.data = "{merchantId: '123456'}";
  EXPECT_EQ(method_data1, method_data2);

  std::vector<std::string> supported_networks1{"visa"};
  method_data1.supported_networks = supported_networks1;
  EXPECT_NE(method_data1, method_data2);
  std::vector<std::string> supported_networks2{"jcb"};
  method_data2.supported_networks = supported_networks2;
  EXPECT_NE(method_data1, method_data2);
  method_data2.supported_networks = supported_networks1;
  EXPECT_EQ(method_data1, method_data2);

  method_data1.supported_types = {autofill::CreditCard::CARD_TYPE_UNKNOWN};
  EXPECT_NE(method_data1, method_data2);
  method_data2.supported_types = {autofill::CreditCard::CARD_TYPE_DEBIT};
  EXPECT_NE(method_data1, method_data2);
  method_data2.supported_types = method_data1.supported_types;
  EXPECT_EQ(method_data1, method_data2);
}

}  // namespace payments
