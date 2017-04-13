// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_request_data_util.h"

#include <memory>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/payments/core/basic_card_response.h"
#include "components/payments/core/payment_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace data_util {

// Tests that the serialized version of the PaymentAddress is according to the
// PaymentAddress spec.
TEST(PaymentRequestDataUtilTest, GetPaymentAddressFromAutofillProfile) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  std::unique_ptr<base::DictionaryValue> address_value =
      payments::data_util::GetPaymentAddressFromAutofillProfile(address,
                                                                "en-US")
          .ToDictionaryValue();
  std::string json_address;
  base::JSONWriter::Write(*address_value, &json_address);
  EXPECT_EQ(
      "{\"addressLine\":[\"666 Erebus St.\",\"Apt 8\"],"
      "\"city\":\"Elysium\","
      "\"country\":\"US\","
      "\"organization\":\"Underworld\","
      "\"phone\":\"16502111111\","
      "\"postalCode\":\"91111\","
      "\"recipient\":\"John H. Doe\","
      "\"region\":\"CA\"}",
      json_address);
}

// Tests that the basic card response constructed from a credit card with
// associated billing address has the right structure once serialized.
TEST(PaymentRequestDataUtilTest, GetBasicCardResponseFromAutofillCreditCard) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(address.guid());
  std::unique_ptr<base::DictionaryValue> response_value =
      payments::data_util::GetBasicCardResponseFromAutofillCreditCard(
          card, base::ASCIIToUTF16("123"),
          std::vector<autofill::AutofillProfile*>{&address}, "en-US")
          .ToDictionaryValue();
  std::string json_response;
  base::JSONWriter::Write(*response_value, &json_response);
  EXPECT_EQ(
      "{\"billingAddress\":"
      "{\"addressLine\":[\"666 Erebus St.\",\"Apt 8\"],"
      "\"city\":\"Elysium\","
      "\"country\":\"US\","
      "\"organization\":\"Underworld\","
      "\"phone\":\"16502111111\","
      "\"postalCode\":\"91111\","
      "\"recipient\":\"John H. Doe\","
      "\"region\":\"CA\"},"
      "\"cardNumber\":\"4111111111111111\","
      "\"cardSecurityCode\":\"123\","
      "\"cardholderName\":\"Test User\","
      "\"expiryMonth\":\"11\","
      "\"expiryYear\":\"2022\"}",
      json_response);
}

// Tests that the phone numbers are correctly formatted for the Payment
// Response.
TEST(PaymentRequestDataUtilTest, FormatPhoneForResponse) {
  EXPECT_EQ("+15151231234", payments::data_util::FormatPhoneForResponse(
                                "(515) 123-1234", "US"));
  EXPECT_EQ("+15151231234", payments::data_util::FormatPhoneForResponse(
                                "(1) 515-123-1234", "US"));
  EXPECT_EQ("+33142685300",
            payments::data_util::FormatPhoneForResponse("1 42 68 53 00", "FR"));
}

// Tests that the phone numbers are correctly formatted to display to the user.
TEST(PaymentRequestDataUtilTest, FormatPhoneForDisplay) {
  EXPECT_EQ("+1 515-123-1234",
            payments::data_util::FormatPhoneForDisplay("5151231234", "US"));
  EXPECT_EQ("+33 1 42 68 53 00",
            payments::data_util::FormatPhoneForDisplay("142685300", "FR"));
}

}  // namespace data_util
}  // namespace payments
