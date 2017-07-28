// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/payments/payment_request.h"

#include <vector>

#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/payments/core/basic_card_response.h"
#include "components/payments/core/payment_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web {

// PaymentRequest parsing tests.

// Tests the success case when populating a PaymentCurrencyAmount from a
// dictionary.
TEST(PaymentRequestTest, PaymentCurrencyAmountFromDictionaryValueSuccess) {
  PaymentCurrencyAmount expected;
  expected.currency = base::ASCIIToUTF16("AUD");
  expected.value = base::ASCIIToUTF16("-438.23");

  base::DictionaryValue amount_dict;
  amount_dict.SetString("currency", "AUD");
  amount_dict.SetString("value", "-438.23");

  PaymentCurrencyAmount actual;
  EXPECT_TRUE(actual.FromDictionaryValue(amount_dict));

  EXPECT_EQ(expected, actual);

  expected.currency_system = base::ASCIIToUTF16("urn:iso:std:iso:123456789");
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

// Tests the success case when populating a PaymentItem from a dictionary.
TEST(PaymentRequestTest, PaymentItemFromDictionaryValueSuccess) {
  PaymentItem expected;
  expected.label = base::ASCIIToUTF16("Payment Total");
  expected.amount.currency = base::ASCIIToUTF16("NZD");
  expected.amount.value = base::ASCIIToUTF16("2,242,093.00");

  base::DictionaryValue item_dict;
  item_dict.SetString("label", "Payment Total");
  std::unique_ptr<base::DictionaryValue> amount_dict(new base::DictionaryValue);
  amount_dict->SetString("currency", "NZD");
  amount_dict->SetString("value", "2,242,093.00");
  item_dict.Set("amount", std::move(amount_dict));

  PaymentItem actual;
  EXPECT_TRUE(actual.FromDictionaryValue(item_dict));

  EXPECT_EQ(expected, actual);
}

// Tests the failure case when populating a PaymentItem from a dictionary.
TEST(PaymentRequestTest, PaymentItemFromDictionaryValueFailure) {
  // Both a label and an amount are required.
  PaymentItem actual;
  base::DictionaryValue item_dict;
  EXPECT_FALSE(actual.FromDictionaryValue(item_dict));

  item_dict.SetString("label", "Payment Total");
  EXPECT_FALSE(actual.FromDictionaryValue(item_dict));

  // Even with both present, the label must be a string.
  std::unique_ptr<base::DictionaryValue> amount_dict(new base::DictionaryValue);
  amount_dict->SetString("currency", "NZD");
  amount_dict->SetString("value", "2,242,093.00");
  item_dict.Set("amount", std::move(amount_dict));
  item_dict.SetInteger("label", 42);
  EXPECT_FALSE(actual.FromDictionaryValue(item_dict));
}

// Tests the success case when populating a PaymentDetails from a dictionary.
TEST(PaymentRequestTest, PaymentDetailsFromDictionaryValueSuccess) {
  PaymentDetails expected;
  expected.error = base::ASCIIToUTF16("Error in details");

  base::DictionaryValue details_dict;
  details_dict.SetString("error", "Error in details");
  PaymentDetails actual;
  EXPECT_TRUE(
      actual.FromDictionaryValue(details_dict, /*requires_total=*/false));
  EXPECT_EQ(expected, actual);

  expected.total.label = base::ASCIIToUTF16("TOTAL");
  expected.total.amount.currency = base::ASCIIToUTF16("GBP");
  expected.total.amount.value = base::ASCIIToUTF16("6.66");

  std::unique_ptr<base::DictionaryValue> total_dict(new base::DictionaryValue);
  total_dict->SetString("label", "TOTAL");
  std::unique_ptr<base::DictionaryValue> amount_dict(new base::DictionaryValue);
  amount_dict->SetString("currency", "GBP");
  amount_dict->SetString("value", "6.66");
  total_dict->Set("amount", std::move(amount_dict));
  details_dict.Set("total", std::move(total_dict));

  EXPECT_TRUE(
      actual.FromDictionaryValue(details_dict, /*requires_total=*/false));
  EXPECT_EQ(expected, actual);

  EXPECT_TRUE(
      actual.FromDictionaryValue(details_dict, /*requires_total=*/true));
  EXPECT_EQ(expected, actual);
}

// Tests the failure case when populating a PaymentDetails from a dictionary.
TEST(PaymentRequestTest, PaymentDetailsFromDictionaryValueFailure) {
  PaymentDetails expected;
  expected.total.label = base::ASCIIToUTF16("TOTAL");
  expected.total.amount.currency = base::ASCIIToUTF16("GBP");
  expected.total.amount.value = base::ASCIIToUTF16("6.66");
  expected.error = base::ASCIIToUTF16("Error in details");

  base::DictionaryValue details_dict;
  details_dict.SetString("error", "Error in details");

  PaymentDetails actual;
  EXPECT_FALSE(
      actual.FromDictionaryValue(details_dict, /*requires_total=*/true));
}

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

// Tests that populating a PaymentRequest from an empty dictionary fails.
TEST(PaymentRequestTest, ParsingEmptyRequestDictionaryFails) {
  PaymentRequest output_request;
  base::DictionaryValue request_dict;
  EXPECT_FALSE(output_request.FromDictionaryValue(request_dict));
}

// Tests that populating a PaymentRequest from a dictionary without all
// required values fails.
TEST(PaymentRequestTest, ParsingPartiallyPopulatedRequestDictionaryFails) {
  PaymentRequest expected_request;
  PaymentRequest output_request;
  base::DictionaryValue request_dict;

  // An empty methodData list alone is insufficient.
  std::unique_ptr<base::ListValue> method_data_list(new base::ListValue);
  request_dict.Set("methodData", std::move(method_data_list));

  EXPECT_FALSE(output_request.FromDictionaryValue(request_dict));
  EXPECT_EQ(expected_request, output_request);

  // A non-dictionary value in the methodData list is incorrect.
  method_data_list.reset(new base::ListValue);
  method_data_list->AppendString("fake method data dictionary");
  request_dict.Set("methodData", std::move(method_data_list));

  EXPECT_FALSE(output_request.FromDictionaryValue(request_dict));
  EXPECT_EQ(expected_request, output_request);

  // An empty dictionary in the methodData list is still insufficient.
  method_data_list.reset(new base::ListValue);
  std::unique_ptr<base::DictionaryValue> method_data_dict(
      new base::DictionaryValue);
  method_data_list->Append(std::move(method_data_dict));
  request_dict.Set("methodData", std::move(method_data_list));

  EXPECT_FALSE(output_request.FromDictionaryValue(request_dict));
  EXPECT_EQ(expected_request, output_request);
}

// Tests that populating a PaymentRequest from a dictionary with all required
// elements succeeds and produces the expected result.
TEST(PaymentRequestTest, ParsingFullyPopulatedRequestDictionarySucceeds) {
  PaymentRequest expected_request;
  PaymentRequest output_request;
  base::DictionaryValue request_dict;

  // Add the expected values to expected_request.
  expected_request.payment_request_id = "123456789";
  expected_request.details.id = "12345";
  expected_request.details.total.label = base::ASCIIToUTF16("TOTAL");
  expected_request.details.total.amount.currency = base::ASCIIToUTF16("GBP");
  expected_request.details.total.amount.value = base::ASCIIToUTF16("6.66");
  expected_request.details.error = base::ASCIIToUTF16("Error in details");

  payments::PaymentMethodData method_data;
  std::vector<std::string> supported_methods;
  supported_methods.push_back("Visa");
  method_data.supported_methods = supported_methods;
  expected_request.method_data.push_back(method_data);

  // Add the same values to the dictionary to be parsed.
  std::unique_ptr<base::DictionaryValue> details_dict(
      new base::DictionaryValue);
  std::unique_ptr<base::DictionaryValue> total_dict(new base::DictionaryValue);
  total_dict->SetString("label", "TOTAL");
  std::unique_ptr<base::DictionaryValue> amount_dict(new base::DictionaryValue);
  amount_dict->SetString("currency", "GBP");
  amount_dict->SetString("value", "6.66");
  total_dict->Set("amount", std::move(amount_dict));
  details_dict->Set("total", std::move(total_dict));
  details_dict->SetString("id", "12345");
  details_dict->SetString("error", "Error in details");
  request_dict.Set("details", std::move(details_dict));

  std::unique_ptr<base::ListValue> method_data_list(new base::ListValue);
  std::unique_ptr<base::DictionaryValue> method_data_dict(
      new base::DictionaryValue);
  std::unique_ptr<base::ListValue> supported_methods_list(new base::ListValue);
  supported_methods_list->AppendString("Visa");
  method_data_dict->Set("supportedMethods", std::move(supported_methods_list));
  method_data_list->Append(std::move(method_data_dict));
  request_dict.Set("methodData", std::move(method_data_list));
  request_dict.SetString("id", "123456789");

  // With the required values present, parsing should succeed.
  EXPECT_TRUE(output_request.FromDictionaryValue(request_dict));
  EXPECT_EQ(expected_request, output_request);

  // If payment options are present, parse those as well.
  std::unique_ptr<base::DictionaryValue> options_dict(
      new base::DictionaryValue);
  options_dict->SetBoolean("requestPayerPhone", true);
  options_dict->SetBoolean("requestShipping", true);
  options_dict->SetString("shippingType", "delivery");
  request_dict.Set("options", std::move(options_dict));

  PaymentOptions payment_options;
  payment_options.request_payer_phone = true;
  payment_options.request_shipping = true;
  payment_options.shipping_type = payments::PaymentShippingType::DELIVERY;
  expected_request.options = payment_options;

  EXPECT_TRUE(output_request.FromDictionaryValue(request_dict));
  EXPECT_EQ(expected_request, output_request);
}

// PaymentRequest serialization tests.

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
  payment_currency_amount.currency = base::ASCIIToUTF16("AUD");
  payment_currency_amount.value = base::ASCIIToUTF16("-438.23");
  payment_currency_amount.currency_system =
      base::ASCIIToUTF16("urn:iso:std:iso:123456789");

  EXPECT_TRUE(
      expected_value.Equals(payment_currency_amount.ToDictionaryValue().get()));
}

// Tests that serializing a default PaymentItem yields the expected result.
TEST(PaymentRequestTest, EmptyPaymentItemDictionary) {
  base::DictionaryValue expected_value;

  expected_value.SetString("label", "");
  std::unique_ptr<base::DictionaryValue> amount_dict =
      base::MakeUnique<base::DictionaryValue>();
  amount_dict->SetString("currency", "");
  amount_dict->SetString("value", "");
  amount_dict->SetString("currencySystem", "urn:iso:std:iso:4217");
  expected_value.SetDictionary("amount", std::move(amount_dict));
  expected_value.SetBoolean("pending", false);

  PaymentItem payment_item;
  EXPECT_TRUE(expected_value.Equals(payment_item.ToDictionaryValue().get()));
}

// Tests that serializing a populated PaymentItem yields the expected result.
TEST(PaymentRequestTest, PopulatedPaymentItemDictionary) {
  base::DictionaryValue expected_value;

  expected_value.SetString("label", "Payment Total");
  std::unique_ptr<base::DictionaryValue> amount_dict =
      base::MakeUnique<base::DictionaryValue>();
  amount_dict->SetString("currency", "NZD");
  amount_dict->SetString("value", "2,242,093.00");
  amount_dict->SetString("currencySystem", "urn:iso:std:iso:4217");
  expected_value.SetDictionary("amount", std::move(amount_dict));
  expected_value.SetBoolean("pending", true);

  PaymentItem payment_item;
  payment_item.label = base::ASCIIToUTF16("Payment Total");
  payment_item.amount.currency = base::ASCIIToUTF16("NZD");
  payment_item.amount.value = base::ASCIIToUTF16("2,242,093.00");
  payment_item.pending = true;

  EXPECT_TRUE(expected_value.Equals(payment_item.ToDictionaryValue().get()));
}

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

// Tests that serializing a default PaymentResponse yields the expected result.
TEST(PaymentRequestTest, EmptyResponseDictionary) {
  base::DictionaryValue expected_value;

  expected_value.SetString("requestId", "");
  expected_value.SetString("methodName", "");
  std::unique_ptr<base::DictionaryValue> shipping_address(
      new base::DictionaryValue);
  shipping_address->SetString("country", "");
  shipping_address->Set("addressLine", base::MakeUnique<base::ListValue>());
  shipping_address->SetString("region", "");
  shipping_address->SetString("dependentLocality", "");
  shipping_address->SetString("city", "");
  shipping_address->SetString("postalCode", "");
  shipping_address->SetString("languageCode", "");
  shipping_address->SetString("sortingCode", "");
  shipping_address->SetString("organization", "");
  shipping_address->SetString("recipient", "");
  shipping_address->SetString("phone", "");
  expected_value.Set("shippingAddress", std::move(shipping_address));

  PaymentResponse payment_response;
  EXPECT_TRUE(
      expected_value.Equals(payment_response.ToDictionaryValue().get()));
}

// Tests that serializing a populated PaymentResponse yields the expected
// result.
TEST(PaymentRequestTest, PopulatedResponseDictionary) {
  base::DictionaryValue expected_value;

  std::unique_ptr<base::DictionaryValue> details(new base::DictionaryValue);
  details->SetString("cardNumber", "1111-1111-1111-1111");
  details->SetString("cardholderName", "Jon Doe");
  details->SetString("expiryMonth", "02");
  details->SetString("expiryYear", "2090");
  details->SetString("cardSecurityCode", "111");
  std::unique_ptr<base::DictionaryValue> billing_address(
      new base::DictionaryValue);
  billing_address->SetString("country", "");
  billing_address->Set("addressLine", base::MakeUnique<base::ListValue>());
  billing_address->SetString("region", "");
  billing_address->SetString("dependentLocality", "");
  billing_address->SetString("city", "");
  billing_address->SetString("postalCode", "90210");
  billing_address->SetString("languageCode", "");
  billing_address->SetString("sortingCode", "");
  billing_address->SetString("organization", "");
  billing_address->SetString("recipient", "");
  billing_address->SetString("phone", "");
  details->Set("billingAddress", std::move(billing_address));
  expected_value.Set("details", std::move(details));
  expected_value.SetString("requestId", "12345");
  expected_value.SetString("methodName", "American Express");
  std::unique_ptr<base::DictionaryValue> shipping_address(
      new base::DictionaryValue);
  shipping_address->SetString("country", "");
  shipping_address->Set("addressLine", base::MakeUnique<base::ListValue>());
  shipping_address->SetString("region", "");
  shipping_address->SetString("dependentLocality", "");
  shipping_address->SetString("city", "");
  shipping_address->SetString("postalCode", "94115");
  shipping_address->SetString("languageCode", "");
  shipping_address->SetString("sortingCode", "");
  shipping_address->SetString("organization", "");
  shipping_address->SetString("recipient", "");
  shipping_address->SetString("phone", "");
  expected_value.Set("shippingAddress", std::move(shipping_address));
  expected_value.SetString("shippingOption", "666");
  expected_value.SetString("payerName", "Jane Doe");
  expected_value.SetString("payerEmail", "jane@example.com");
  expected_value.SetString("payerPhone", "1234-567-890");

  PaymentResponse payment_response;
  payment_response.payment_request_id = "12345";
  payment_response.method_name = "American Express";

  payments::BasicCardResponse payment_response_details;
  payment_response_details.card_number =
      base::ASCIIToUTF16("1111-1111-1111-1111");
  payment_response_details.cardholder_name = base::ASCIIToUTF16("Jon Doe");
  payment_response_details.expiry_month = base::ASCIIToUTF16("02");
  payment_response_details.expiry_year = base::ASCIIToUTF16("2090");
  payment_response_details.card_security_code = base::ASCIIToUTF16("111");
  payment_response_details.billing_address.postal_code =
      base::ASCIIToUTF16("90210");
  std::unique_ptr<base::DictionaryValue> response_value =
      payment_response_details.ToDictionaryValue();
  std::string payment_response_stringified_details;
  base::JSONWriter::Write(*response_value,
                          &payment_response_stringified_details);
  payment_response.details = payment_response_stringified_details;

  payment_response.shipping_address.postal_code = base::ASCIIToUTF16("94115");
  payment_response.shipping_option = base::ASCIIToUTF16("666");
  payment_response.payer_name = base::ASCIIToUTF16("Jane Doe");
  payment_response.payer_email = base::ASCIIToUTF16("jane@example.com");
  payment_response.payer_phone = base::ASCIIToUTF16("1234-567-890");
  EXPECT_TRUE(
      expected_value.Equals(payment_response.ToDictionaryValue().get()));
}

// Value equality tests.

// Tests that two currency amount objects are not equal if their property values
// differ or one is missing a value present in the other, and equal otherwise.
TEST(PaymentRequestTest, PaymentCurrencyAmountEquality) {
  PaymentCurrencyAmount currency_amount1;
  PaymentCurrencyAmount currency_amount2;
  EXPECT_EQ(currency_amount1, currency_amount2);

  currency_amount1.currency = base::ASCIIToUTF16("HKD");
  EXPECT_NE(currency_amount1, currency_amount2);
  currency_amount2.currency = base::ASCIIToUTF16("USD");
  EXPECT_NE(currency_amount1, currency_amount2);
  currency_amount2.currency = base::ASCIIToUTF16("HKD");
  EXPECT_EQ(currency_amount1, currency_amount2);

  currency_amount1.value = base::ASCIIToUTF16("49.89");
  EXPECT_NE(currency_amount1, currency_amount2);
  currency_amount2.value = base::ASCIIToUTF16("49.99");
  EXPECT_NE(currency_amount1, currency_amount2);
  currency_amount2.value = base::ASCIIToUTF16("49.89");
  EXPECT_EQ(currency_amount1, currency_amount2);
}

// Tests that two payment item objects are not equal if their property values
// differ or one is missing a value present in the other, and equal otherwise.
// Doesn't test all properties of child objects, relying instead on their
// respective tests.
TEST(PaymentRequestTest, PaymentItemEquality) {
  PaymentItem item1;
  PaymentItem item2;
  EXPECT_EQ(item1, item2);

  item1.label = base::ASCIIToUTF16("Subtotal");
  EXPECT_NE(item1, item2);
  item2.label = base::ASCIIToUTF16("Total");
  EXPECT_NE(item1, item2);
  item2.label = base::ASCIIToUTF16("Subtotal");
  EXPECT_EQ(item1, item2);

  item1.amount.value = base::ASCIIToUTF16("104.34");
  EXPECT_NE(item1, item2);
  item2.amount.value = base::ASCIIToUTF16("104");
  EXPECT_NE(item1, item2);
  item2.amount.value = base::ASCIIToUTF16("104.34");
  EXPECT_EQ(item1, item2);

  item1.pending = true;
  EXPECT_NE(item1, item2);
  item2.pending = true;
  EXPECT_EQ(item1, item2);
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

// Tests that two payment details objects are not equal if their property values
// differ or one is missing a value present in the other, and equal otherwise.
// Doesn't test all properties of child objects, relying instead on their
// respective tests.
TEST(PaymentRequestTest, PaymentDetailsEquality) {
  PaymentDetails details1;
  PaymentDetails details2;
  EXPECT_EQ(details1, details2);

  details1.id = "12345";
  EXPECT_NE(details1, details2);
  details2.id = "54321";
  EXPECT_NE(details1, details2);
  details2.id = details1.id;
  EXPECT_EQ(details1, details2);

  details1.total.label = base::ASCIIToUTF16("Total");
  EXPECT_NE(details1, details2);
  details2.total.label = base::ASCIIToUTF16("Shipping");
  EXPECT_NE(details1, details2);
  details2.total.label = base::ASCIIToUTF16("Total");
  EXPECT_EQ(details1, details2);

  details1.error = base::ASCIIToUTF16("Foo");
  EXPECT_NE(details1, details2);
  details2.error = base::ASCIIToUTF16("Bar");
  EXPECT_NE(details1, details2);
  details2.error = base::ASCIIToUTF16("Foo");
  EXPECT_EQ(details1, details2);

  PaymentItem payment_item;
  payment_item.label = base::ASCIIToUTF16("Tax");
  std::vector<PaymentItem> display_items1;
  display_items1.push_back(payment_item);
  details1.display_items = display_items1;
  EXPECT_NE(details1, details2);
  std::vector<PaymentItem> display_items2;
  display_items2.push_back(payment_item);
  display_items2.push_back(payment_item);
  details2.display_items = display_items2;
  EXPECT_NE(details1, details2);
  details2.display_items = display_items1;
  EXPECT_EQ(details1, details2);

  PaymentShippingOption shipping_option;
  shipping_option.label = base::ASCIIToUTF16("Overnight");
  std::vector<PaymentShippingOption> shipping_options1;
  shipping_options1.push_back(shipping_option);
  details1.shipping_options = shipping_options1;
  EXPECT_NE(details1, details2);
  std::vector<PaymentShippingOption> shipping_options2;
  shipping_options2.push_back(shipping_option);
  shipping_options2.push_back(shipping_option);
  details2.shipping_options = shipping_options2;
  EXPECT_NE(details1, details2);
  details2.shipping_options = shipping_options1;
  EXPECT_EQ(details1, details2);

  PaymentDetailsModifier details_modifier;
  details_modifier.total.label = base::ASCIIToUTF16("Total");
  std::vector<PaymentDetailsModifier> details_modifiers1;
  details_modifiers1.push_back(details_modifier);
  details1.modifiers = details_modifiers1;
  EXPECT_NE(details1, details2);
  std::vector<PaymentDetailsModifier> details_modifiers2;
  details2.modifiers = details_modifiers2;
  EXPECT_NE(details1, details2);
  details2.modifiers = details_modifiers1;
  EXPECT_EQ(details1, details2);
}

// Tests that two payment options objects are not equal if their property values
// differ and equal otherwise.
TEST(PaymentRequestTest, PaymentOptionsEquality) {
  PaymentOptions options1;
  PaymentOptions options2;
  EXPECT_EQ(options1, options2);

  options1.request_payer_name = true;
  EXPECT_NE(options1, options2);
  options2.request_payer_name = true;
  EXPECT_EQ(options1, options2);

  options1.request_payer_email = true;
  EXPECT_NE(options1, options2);
  options2.request_payer_email = true;
  EXPECT_EQ(options1, options2);

  options1.request_payer_phone = true;
  EXPECT_NE(options1, options2);
  options2.request_payer_phone = true;
  EXPECT_EQ(options1, options2);

  options1.request_shipping = true;
  EXPECT_NE(options1, options2);
  options2.request_shipping = true;
  EXPECT_EQ(options1, options2);

  // payments::PaymentShippingType::SHIPPING is the default value for
  // shipping_type.
  options1.shipping_type = payments::PaymentShippingType::SHIPPING;
  EXPECT_EQ(options1, options2);
  options1.shipping_type = payments::PaymentShippingType::PICKUP;
  EXPECT_NE(options1, options2);
  options2.shipping_type = payments::PaymentShippingType::PICKUP;
  EXPECT_EQ(options1, options2);
}

// Tests that two payment request objects are not equal if their property values
// differ or one is missing a value present in the other, and equal otherwise.
// Doesn't test all properties of child objects, relying instead on their
// respective tests.
TEST(PaymentRequestTest, PaymentRequestEquality) {
  PaymentRequest request1;
  PaymentRequest request2;
  EXPECT_EQ(request1, request2);

  request1.payment_request_id = "12345";
  EXPECT_NE(request1, request2);
  request2.payment_request_id = "54321";
  EXPECT_NE(request1, request2);
  request2.payment_request_id = request1.payment_request_id;
  EXPECT_EQ(request1, request2);

  payments::PaymentAddress address1;
  address1.recipient = base::ASCIIToUTF16("Jessica Jones");
  request1.shipping_address = address1;
  EXPECT_NE(request1, request2);
  payments::PaymentAddress address2;
  address2.recipient = base::ASCIIToUTF16("Luke Cage");
  request2.shipping_address = address2;
  EXPECT_NE(request1, request2);
  request2.shipping_address = address1;
  EXPECT_EQ(request1, request2);

  request1.shipping_option = base::ASCIIToUTF16("2-Day");
  EXPECT_NE(request1, request2);
  request2.shipping_option = base::ASCIIToUTF16("3-Day");
  EXPECT_NE(request1, request2);
  request2.shipping_option = base::ASCIIToUTF16("2-Day");
  EXPECT_EQ(request1, request2);

  payments::PaymentMethodData method_datum;
  method_datum.data = "{merchantId: '123456'}";
  std::vector<payments::PaymentMethodData> method_data1;
  method_data1.push_back(method_datum);
  request1.method_data = method_data1;
  EXPECT_NE(request1, request2);
  std::vector<payments::PaymentMethodData> method_data2;
  request2.method_data = method_data2;
  EXPECT_NE(request1, request2);
  request2.method_data = method_data1;
  EXPECT_EQ(request1, request2);

  PaymentDetails details1;
  details1.total.label = base::ASCIIToUTF16("Total");
  request1.details = details1;
  EXPECT_NE(request1, request2);
  PaymentDetails details2;
  details2.total.amount.value = base::ASCIIToUTF16("0.01");
  request2.details = details2;
  EXPECT_NE(request1, request2);
  request2.details = details1;
  EXPECT_EQ(request1, request2);

  PaymentOptions options;
  options.request_shipping = true;
  request1.options = options;
  EXPECT_NE(request1, request2);
  request2.options = options;
  EXPECT_EQ(request1, request2);
}

// Tests that two payment response objects are not equal if their property
// values differ or one is missing a value present in the other, and equal
// otherwise. Doesn't test all properties of child objects, relying instead on
// their respective tests.
TEST(PaymentRequestTest, PaymentResponseEquality) {
  PaymentResponse response1;
  PaymentResponse response2;
  EXPECT_EQ(response1, response2);

  response1.method_name = "Visa";
  EXPECT_NE(response1, response2);
  response2.method_name = "Mastercard";
  EXPECT_NE(response1, response2);
  response2.method_name = "Visa";
  EXPECT_EQ(response1, response2);

  payments::BasicCardResponse card_response1;
  card_response1.card_number = base::ASCIIToUTF16("1234");
  std::unique_ptr<base::DictionaryValue> response_value1 =
      card_response1.ToDictionaryValue();
  std::string stringified_card_response1;
  base::JSONWriter::Write(*response_value1, &stringified_card_response1);
  payments::BasicCardResponse card_response2;
  card_response2.card_number = base::ASCIIToUTF16("8888");
  std::unique_ptr<base::DictionaryValue> response_value2 =
      card_response2.ToDictionaryValue();
  std::string stringified_card_response2;
  base::JSONWriter::Write(*response_value2, &stringified_card_response2);
  response1.details = stringified_card_response1;
  EXPECT_NE(response1, response2);
  response2.details = stringified_card_response2;
  EXPECT_NE(response1, response2);
  response2.details = stringified_card_response1;
  EXPECT_EQ(response1, response2);
}

}  // namespace web