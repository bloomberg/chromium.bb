// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/payments/payment_request.h"

#include <vector>

#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {

// PaymentRequest parsing tests.

using PaymentRequestTest = PlatformTest;

// Tests that populating a PaymentRequest from an empty dictionary fails.
TEST_F(PaymentRequestTest, ParsingEmptyRequestDictionaryFails) {
  PaymentRequest output_request;
  base::DictionaryValue request_dict;
  EXPECT_FALSE(output_request.FromDictionaryValue(request_dict));
}

// Tests that populating a PaymentRequest from a dictionary without all
// required values fails.
TEST_F(PaymentRequestTest, ParsingPartiallyPopulatedRequestDictionaryFails) {
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
TEST_F(PaymentRequestTest, ParsingFullyPopulatedRequestDictionarySucceeds) {
  PaymentRequest expected_request;
  PaymentRequest output_request;
  base::DictionaryValue request_dict;

  // Add the expected values to expected_request.
  expected_request.payment_request_id = "123456789";
  expected_request.details.id = "12345";
  expected_request.details.total = base::MakeUnique<payments::PaymentItem>();
  expected_request.details.total->label = "TOTAL";
  expected_request.details.total->amount.currency = "GBP";
  expected_request.details.total->amount.value = "6.66";
  expected_request.details.error = "Error in details";

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

// Tests that serializing a default PaymentResponse yields the expected result.
TEST_F(PaymentRequestTest, EmptyResponseDictionary) {
  base::DictionaryValue expected_value;

  expected_value.SetString("requestId", "");
  expected_value.SetString("methodName", "");
  expected_value.Set("details", base::MakeUnique<base::Value>());
  expected_value.Set("shippingAddress", base::MakeUnique<base::Value>());
  expected_value.SetString("shippingOption", "");
  expected_value.SetString("payerName", "");
  expected_value.SetString("payerEmail", "");
  expected_value.SetString("payerPhone", "");

  PaymentResponse payment_response;
  EXPECT_TRUE(
      expected_value.Equals(payment_response.ToDictionaryValue().get()));
}

// Tests that serializing a populated PaymentResponse yields the expected
// result.
TEST_F(PaymentRequestTest, PopulatedResponseDictionary) {
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

  payment_response.shipping_address =
      base::MakeUnique<payments::PaymentAddress>();
  payment_response.shipping_address->postal_code = base::ASCIIToUTF16("94115");
  payment_response.shipping_option = "666";
  payment_response.payer_name = base::ASCIIToUTF16("Jane Doe");
  payment_response.payer_email = base::ASCIIToUTF16("jane@example.com");
  payment_response.payer_phone = base::ASCIIToUTF16("1234-567-890");
  EXPECT_TRUE(
      expected_value.Equals(payment_response.ToDictionaryValue().get()));
}

// Value equality tests.

// Tests that two payment options objects are not equal if their property values
// differ and equal otherwise.
TEST_F(PaymentRequestTest, PaymentOptionsEquality) {
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
TEST_F(PaymentRequestTest, PaymentRequestEquality) {
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

  request1.shipping_option = "2-Day";
  EXPECT_NE(request1, request2);
  request2.shipping_option = "3-Day";
  EXPECT_NE(request1, request2);
  request2.shipping_option = "2-Day";
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

  payments::PaymentDetails details1;
  details1.total = base::MakeUnique<payments::PaymentItem>();
  details1.total->label = "Total";
  request1.details = details1;
  EXPECT_NE(request1, request2);
  payments::PaymentDetails details2;
  details2.total = base::MakeUnique<payments::PaymentItem>();
  details2.total->amount.value = "0.01";
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
TEST_F(PaymentRequestTest, PaymentResponseEquality) {
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
