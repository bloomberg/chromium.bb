// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/payments/payment_request.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web {

// PaymentRequest parsing tests.

// Tests the success case when populating a PaymentMethodData from a dictionary.
TEST(PaymentRequestTest, PaymentMethodDataFromDictionaryValueSuccess) {
  PaymentMethodData expected;
  std::vector<base::string16> supported_methods;
  supported_methods.push_back(base::ASCIIToUTF16("Visa"));
  supported_methods.push_back(base::ASCIIToUTF16("Bitcoin"));
  expected.supported_methods = supported_methods;
  expected.data = base::ASCIIToUTF16("{merchantId: 'af22fke9'}");

  base::DictionaryValue method_data_dict;
  std::unique_ptr<base::ListValue> supported_methods_list(new base::ListValue);
  supported_methods_list->AppendString("Visa");
  supported_methods_list->AppendString("Bitcoin");
  method_data_dict.Set("supportedMethods", std::move(supported_methods_list));
  method_data_dict.SetString("data", "{merchantId: 'af22fke9'}");

  PaymentMethodData actual;
  EXPECT_TRUE(actual.FromDictionaryValue(method_data_dict));

  EXPECT_EQ(expected, actual);
}

// Tests the failure case when populating a PaymentMethodData from a dictionary.
TEST(PaymentRequestTest, PaymentMethodDataFromDictionaryValueFailure) {
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
  expected_request.details.total.label = base::ASCIIToUTF16("TOTAL");
  expected_request.details.total.amount.currency = base::ASCIIToUTF16("GBP");
  expected_request.details.total.amount.value = base::ASCIIToUTF16("6.66");
  expected_request.details.error = base::ASCIIToUTF16("Error in details");

  PaymentMethodData method_data;
  std::vector<base::string16> supported_methods;
  supported_methods.push_back(base::ASCIIToUTF16("Visa"));
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
  payment_options.shipping_type = PaymentShippingType::DELIVERY;
  expected_request.options = payment_options;

  EXPECT_TRUE(output_request.FromDictionaryValue(request_dict));
  EXPECT_EQ(expected_request, output_request);
}

// PaymentResponse serialization tests.

// Tests that serializing a default PaymentResponse yields the expected result.
TEST(PaymentRequestTest, EmptyResponseDictionary) {
  base::DictionaryValue expected_value;

  std::unique_ptr<base::DictionaryValue> details(new base::DictionaryValue);
  details->SetString("cardNumber", "");
  expected_value.Set("details", std::move(details));
  expected_value.SetString("paymentRequestID", "");
  expected_value.SetString("methodName", "");

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
  billing_address->SetString("postalCode", "90210");
  details->Set("billingAddress", std::move(billing_address));
  expected_value.Set("details", std::move(details));
  expected_value.SetString("paymentRequestID", "12345");
  expected_value.SetString("methodName", "American Express");
  std::unique_ptr<base::DictionaryValue> shipping_address(
      new base::DictionaryValue);
  shipping_address->SetString("postalCode", "94115");
  expected_value.Set("shippingAddress", std::move(shipping_address));
  expected_value.SetString("shippingOption", "666");
  expected_value.SetString("payerName", "Jane Doe");
  expected_value.SetString("payerEmail", "jane@example.com");
  expected_value.SetString("payerPhone", "1234-567-890");

  PaymentResponse payment_response;
  payment_response.payment_request_id = base::ASCIIToUTF16("12345");
  payment_response.method_name = base::ASCIIToUTF16("American Express");
  payment_response.details.card_number =
      base::ASCIIToUTF16("1111-1111-1111-1111");
  payment_response.details.cardholder_name = base::ASCIIToUTF16("Jon Doe");
  payment_response.details.expiry_month = base::ASCIIToUTF16("02");
  payment_response.details.expiry_year = base::ASCIIToUTF16("2090");
  payment_response.details.card_security_code = base::ASCIIToUTF16("111");
  payment_response.details.billing_address.postal_code =
      base::ASCIIToUTF16("90210");
  payment_response.shipping_address.postal_code = base::ASCIIToUTF16("94115");
  payment_response.shipping_option = base::ASCIIToUTF16("666");
  payment_response.payer_name = base::ASCIIToUTF16("Jane Doe");
  payment_response.payer_email = base::ASCIIToUTF16("jane@example.com");
  payment_response.payer_phone = base::ASCIIToUTF16("1234-567-890");
  EXPECT_TRUE(
      expected_value.Equals(payment_response.ToDictionaryValue().get()));
}

// Value equality tests.

// Tests that two addresses are not equal if their property values differ or
// one is missing a value present in the other, and equal otherwise.
TEST(PaymentRequestTest, PaymentAddressEquality) {
  PaymentAddress address1;
  PaymentAddress address2;
  EXPECT_EQ(address1, address2);

  address1.country = base::ASCIIToUTF16("Madagascar");
  EXPECT_NE(address1, address2);
  address2.country = base::ASCIIToUTF16("Monaco");
  EXPECT_NE(address1, address2);
  address2.country = base::ASCIIToUTF16("Madagascar");
  EXPECT_EQ(address1, address2);

  std::vector<base::string16> address_line1;
  address_line1.push_back(base::ASCIIToUTF16("123 Main St."));
  address_line1.push_back(base::ASCIIToUTF16("Apartment B"));
  address1.address_line = address_line1;
  EXPECT_NE(address1, address2);
  std::vector<base::string16> address_line2;
  address_line2.push_back(base::ASCIIToUTF16("123 Main St."));
  address_line2.push_back(base::ASCIIToUTF16("Apartment C"));
  address2.address_line = address_line2;
  EXPECT_NE(address1, address2);
  address2.address_line = address_line1;
  EXPECT_EQ(address1, address2);

  address1.region = base::ASCIIToUTF16("Quebec");
  EXPECT_NE(address1, address2);
  address2.region = base::ASCIIToUTF16("Newfoundland and Labrador");
  EXPECT_NE(address1, address2);
  address2.region = base::ASCIIToUTF16("Quebec");
  EXPECT_EQ(address1, address2);

  address1.city = base::ASCIIToUTF16("Timbuktu");
  EXPECT_NE(address1, address2);
  address2.city = base::ASCIIToUTF16("Timbuk 3");
  EXPECT_NE(address1, address2);
  address2.city = base::ASCIIToUTF16("Timbuktu");
  EXPECT_EQ(address1, address2);

  address1.dependent_locality = base::ASCIIToUTF16("Manhattan");
  EXPECT_NE(address1, address2);
  address2.dependent_locality = base::ASCIIToUTF16("Queens");
  EXPECT_NE(address1, address2);
  address2.dependent_locality = base::ASCIIToUTF16("Manhattan");
  EXPECT_EQ(address1, address2);

  address1.postal_code = base::ASCIIToUTF16("90210");
  EXPECT_NE(address1, address2);
  address2.postal_code = base::ASCIIToUTF16("89049");
  EXPECT_NE(address1, address2);
  address2.postal_code = base::ASCIIToUTF16("90210");
  EXPECT_EQ(address1, address2);

  address1.sorting_code = base::ASCIIToUTF16("14390");
  EXPECT_NE(address1, address2);
  address2.sorting_code = base::ASCIIToUTF16("09341");
  EXPECT_NE(address1, address2);
  address2.sorting_code = base::ASCIIToUTF16("14390");
  EXPECT_EQ(address1, address2);

  address1.language_code = base::ASCIIToUTF16("fr");
  EXPECT_NE(address1, address2);
  address2.language_code = base::ASCIIToUTF16("zh-HK");
  EXPECT_NE(address1, address2);
  address2.language_code = base::ASCIIToUTF16("fr");
  EXPECT_EQ(address1, address2);

  address1.organization = base::ASCIIToUTF16("The Willy Wonka Candy Company");
  EXPECT_NE(address1, address2);
  address2.organization = base::ASCIIToUTF16("Sears");
  EXPECT_NE(address1, address2);
  address2.organization = base::ASCIIToUTF16("The Willy Wonka Candy Company");
  EXPECT_EQ(address1, address2);

  address1.recipient = base::ASCIIToUTF16("Veruca Salt");
  EXPECT_NE(address1, address2);
  address2.recipient = base::ASCIIToUTF16("Veronica Mars");
  EXPECT_NE(address1, address2);
  address2.recipient = base::ASCIIToUTF16("Veruca Salt");
  EXPECT_EQ(address1, address2);

  address1.care_of = base::ASCIIToUTF16("Jarvis");
  EXPECT_NE(address1, address2);
  address2.care_of = base::ASCIIToUTF16("Tony");
  EXPECT_NE(address1, address2);
  address2.care_of = base::ASCIIToUTF16("Jarvis");
  EXPECT_EQ(address1, address2);

  address1.phone = base::ASCIIToUTF16("888-867-5309");
  EXPECT_NE(address1, address2);
  address2.phone = base::ASCIIToUTF16("800-984-3672");
  EXPECT_NE(address1, address2);
  address2.phone = base::ASCIIToUTF16("888-867-5309");
  EXPECT_EQ(address1, address2);
}

// Tests that two method data objects are not equal if their property values
// differ or one is missing a value present in the other, and equal otherwise.
TEST(PaymentRequestTest, PaymentMethodDataEquality) {
  PaymentMethodData method_data1;
  PaymentMethodData method_data2;
  EXPECT_EQ(method_data1, method_data2);

  std::vector<base::string16> supported_methods1;
  supported_methods1.push_back(base::ASCIIToUTF16("Visa"));
  supported_methods1.push_back(base::ASCIIToUTF16("BobPay"));
  method_data1.supported_methods = supported_methods1;
  EXPECT_NE(method_data1, method_data2);
  std::vector<base::string16> supported_methods2;
  supported_methods2.push_back(base::ASCIIToUTF16("BobPay"));
  method_data2.supported_methods = supported_methods2;
  EXPECT_NE(method_data1, method_data2);
  method_data2.supported_methods = supported_methods1;
  EXPECT_EQ(method_data1, method_data2);

  method_data1.data = base::ASCIIToUTF16("{merchantId: '123456'}");
  EXPECT_NE(method_data1, method_data2);
  method_data2.data = base::ASCIIToUTF16("{merchantId: '9999-88'}");
  EXPECT_NE(method_data1, method_data2);
  method_data2.data = base::ASCIIToUTF16("{merchantId: '123456'}");
  EXPECT_EQ(method_data1, method_data2);
}

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

  // PaymentShippingType::SHIPPING is the default value for shipping_type.
  options1.shipping_type = PaymentShippingType::SHIPPING;
  EXPECT_EQ(options1, options2);
  options1.shipping_type = PaymentShippingType::PICKUP;
  EXPECT_NE(options1, options2);
  options2.shipping_type = PaymentShippingType::PICKUP;
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

  PaymentAddress address1;
  address1.recipient = base::ASCIIToUTF16("Jessica Jones");
  request1.shipping_address = address1;
  EXPECT_NE(request1, request2);
  PaymentAddress address2;
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

  PaymentMethodData method_datum;
  method_datum.data = base::ASCIIToUTF16("{merchantId: '123456'}");
  std::vector<PaymentMethodData> method_data1;
  method_data1.push_back(method_datum);
  request1.method_data = method_data1;
  EXPECT_NE(request1, request2);
  std::vector<PaymentMethodData> method_data2;
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

// Tests that two credit card response objects are not equal if their property
// values differ or one is missing a value present in the other, and equal
// otherwise. Doesn't test all properties of child objects, relying instead on
// their respective tests.
TEST(PaymentRequestTest, BasicCardResponseEquality) {
  BasicCardResponse card_response1;
  BasicCardResponse card_response2;
  EXPECT_EQ(card_response1, card_response2);

  card_response1.cardholder_name = base::ASCIIToUTF16("Shadow Moon");
  EXPECT_NE(card_response1, card_response2);
  card_response2.cardholder_name = base::ASCIIToUTF16("Mad Sweeney");
  EXPECT_NE(card_response1, card_response2);
  card_response2.cardholder_name = base::ASCIIToUTF16("Shadow Moon");
  EXPECT_EQ(card_response1, card_response2);

  card_response1.card_number = base::ASCIIToUTF16("4111111111111111");
  EXPECT_NE(card_response1, card_response2);
  card_response2.card_number = base::ASCIIToUTF16("1111");
  EXPECT_NE(card_response1, card_response2);
  card_response2.card_number = base::ASCIIToUTF16("4111111111111111");
  EXPECT_EQ(card_response1, card_response2);

  card_response1.expiry_month = base::ASCIIToUTF16("01");
  EXPECT_NE(card_response1, card_response2);
  card_response2.expiry_month = base::ASCIIToUTF16("11");
  EXPECT_NE(card_response1, card_response2);
  card_response2.expiry_month = base::ASCIIToUTF16("01");
  EXPECT_EQ(card_response1, card_response2);

  card_response1.expiry_year = base::ASCIIToUTF16("27");
  EXPECT_NE(card_response1, card_response2);
  card_response2.expiry_year = base::ASCIIToUTF16("72");
  EXPECT_NE(card_response1, card_response2);
  card_response2.expiry_year = base::ASCIIToUTF16("27");
  EXPECT_EQ(card_response1, card_response2);

  card_response1.expiry_year = base::ASCIIToUTF16("123");
  EXPECT_NE(card_response1, card_response2);
  card_response2.expiry_year = base::ASCIIToUTF16("999");
  EXPECT_NE(card_response1, card_response2);
  card_response2.expiry_year = base::ASCIIToUTF16("123");
  EXPECT_EQ(card_response1, card_response2);

  PaymentAddress billing_address1;
  billing_address1.postal_code = base::ASCIIToUTF16("90210");
  PaymentAddress billing_address2;
  billing_address2.postal_code = base::ASCIIToUTF16("01209");
  card_response1.billing_address = billing_address1;
  EXPECT_NE(card_response1, card_response2);
  card_response2.billing_address = billing_address2;
  EXPECT_NE(card_response1, card_response2);
  card_response2.billing_address = billing_address1;
  EXPECT_EQ(card_response1, card_response2);
}

// Tests that two payment response objects are not equal if their property
// values differ or one is missing a value present in the other, and equal
// otherwise. Doesn't test all properties of child objects, relying instead on
// their respective tests.
TEST(PaymentRequestTest, PaymentResponseEquality) {
  PaymentResponse response1;
  PaymentResponse response2;
  EXPECT_EQ(response1, response2);

  response1.method_name = base::ASCIIToUTF16("Visa");
  EXPECT_NE(response1, response2);
  response2.method_name = base::ASCIIToUTF16("Mastercard");
  EXPECT_NE(response1, response2);
  response2.method_name = base::ASCIIToUTF16("Visa");
  EXPECT_EQ(response1, response2);

  BasicCardResponse card_response1;
  card_response1.card_number = base::ASCIIToUTF16("1234");
  BasicCardResponse card_response2;
  card_response2.card_number = base::ASCIIToUTF16("8888");
  response1.details = card_response1;
  EXPECT_NE(response1, response2);
  response2.details = card_response2;
  EXPECT_NE(response1, response2);
  response2.details = card_response1;
  EXPECT_EQ(response1, response2);
}

}  // namespace web
