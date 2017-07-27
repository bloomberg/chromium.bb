// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_request_data_util.h"

#include <memory>

#include "base/json/json_writer.h"
#include "base/macros.h"
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
      "\"dependentLocality\":\"\","
      "\"languageCode\":\"\","
      "\"organization\":\"Underworld\","
      "\"phone\":\"16502111111\","
      "\"postalCode\":\"91111\","
      "\"recipient\":\"John H. Doe\","
      "\"region\":\"CA\","
      "\"sortingCode\":\"\"}",
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
          card, base::ASCIIToUTF16("123"), address, "en-US")
          .ToDictionaryValue();
  std::string json_response;
  base::JSONWriter::Write(*response_value, &json_response);
  EXPECT_EQ(
      "{\"billingAddress\":"
      "{\"addressLine\":[\"666 Erebus St.\",\"Apt 8\"],"
      "\"city\":\"Elysium\","
      "\"country\":\"US\","
      "\"dependentLocality\":\"\","
      "\"languageCode\":\"\","
      "\"organization\":\"Underworld\","
      "\"phone\":\"16502111111\","
      "\"postalCode\":\"91111\","
      "\"recipient\":\"John H. Doe\","
      "\"region\":\"CA\","
      "\"sortingCode\":\"\"},"
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

// Test for the GetFormattedPhoneNumberForDisplay method.
struct PhoneNumberFormatCase {
  PhoneNumberFormatCase(const char* phone,
                        const char* country,
                        const char* expected_format,
                        const char* locale = "")
      : phone(phone),
        country(country),
        expected_format(expected_format),
        locale(locale) {}

  const char* phone;
  const char* country;
  const char* expected_format;
  const char* locale;
};

class GetFormattedPhoneNumberForDisplayTest
    : public testing::TestWithParam<PhoneNumberFormatCase> {};

TEST_P(GetFormattedPhoneNumberForDisplayTest,
       GetFormattedPhoneNumberForDisplay) {
  autofill::AutofillProfile profile;
  profile.SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER,
                     base::UTF8ToUTF16(GetParam().phone));
  profile.SetRawInfo(autofill::ADDRESS_HOME_COUNTRY,
                     base::UTF8ToUTF16(GetParam().country));
  EXPECT_EQ(GetParam().expected_format,
            base::UTF16ToUTF8(
                GetFormattedPhoneNumberForDisplay(profile, GetParam().locale)));
}

INSTANTIATE_TEST_CASE_P(
    GetFormattedPhoneNumberForDisplay,
    GetFormattedPhoneNumberForDisplayTest,
    testing::Values(
        //////////////////////////
        // US phone in US.
        //////////////////////////
        // Formatted phone numbers.
        PhoneNumberFormatCase("+1 415-555-5555", "US", "+1 415-555-5555"),
        PhoneNumberFormatCase("1 415-555-5555", "US", "+1 415-555-5555"),
        PhoneNumberFormatCase("415-555-5555", "US", "+1 415-555-5555"),
        // Raw phone numbers.
        PhoneNumberFormatCase("+14155555555", "US", "+1 415-555-5555"),
        PhoneNumberFormatCase("14155555555", "US", "+1 415-555-5555"),
        PhoneNumberFormatCase("4155555555", "US", "+1 415-555-5555"),

        //////////////////////////
        // US phone in CA.
        //////////////////////////
        // Formatted phone numbers.
        PhoneNumberFormatCase("+1 415-555-5555", "CA", "+1 415-555-5555"),
        PhoneNumberFormatCase("1 415-555-5555", "CA", "+1 415-555-5555"),
        PhoneNumberFormatCase("415-555-5555", "CA", "+1 415-555-5555"),
        // Raw phone numbers.
        PhoneNumberFormatCase("+14155555555", "CA", "+1 415-555-5555"),
        PhoneNumberFormatCase("14155555555", "CA", "+1 415-555-5555"),
        PhoneNumberFormatCase("4155555555", "CA", "+1 415-555-5555"),

        //////////////////////////
        // US phone in AU.
        //////////////////////////
        // A US phone with the country code is correctly formatted as an US
        // number.
        PhoneNumberFormatCase("+1 415-555-5555", "AU", "+1 415-555-5555"),
        PhoneNumberFormatCase("1 415-555-5555", "AU", "+1 415-555-5555"),
        // Without a country code, the phone is formatted for the profile's
        // country, even if it's invalid.
        PhoneNumberFormatCase("415-555-5555", "AU", "+61 4155555555"),

        //////////////////////////
        // US phone in MX.
        //////////////////////////
        // A US phone with the country code is correctly formatted as an US
        // number.
        PhoneNumberFormatCase("+1 415-555-5555", "MX", "+1 415-555-5555"),
        // "+52 1 415 555 5555" is a valid number for Mexico,
        PhoneNumberFormatCase("1 415-555-5555", "MX", "+52 1 415 555 5555"),
        // Without a country code, the phone is formatted for the profile's
        // country.
        PhoneNumberFormatCase("415-555-5555", "MX", "+52 415 555 5555"),

        //////////////////////////
        // AU phone in AU.
        //////////////////////////
        // Formatted phone numbers.
        PhoneNumberFormatCase("+61 2 9374 4000", "AU", "+61 2 9374 4000"),
        PhoneNumberFormatCase("61 2 9374 4000", "AU", "+61 2 9374 4000"),
        PhoneNumberFormatCase("02 9374 4000", "AU", "+61 2 9374 4000"),
        PhoneNumberFormatCase("2 9374 4000", "AU", "+61 2 9374 4000"),
        // Raw phone numbers.
        PhoneNumberFormatCase("+61293744000", "AU", "+61 2 9374 4000"),
        PhoneNumberFormatCase("61293744000", "AU", "+61 2 9374 4000"),
        PhoneNumberFormatCase("0293744000", "AU", "+61 2 9374 4000"),
        PhoneNumberFormatCase("293744000", "AU", "+61 2 9374 4000"),

        //////////////////////////
        // AU phone in US.
        //////////////////////////
        // An AU phone with the country code is correctly formatted as an AU
        // number.
        PhoneNumberFormatCase("+61 2 9374 4000", "US", "+61 2 9374 4000"),
        PhoneNumberFormatCase("61 2 9374 4000", "US", "+61 2 9374 4000"),
        // Without a country code, the phone is formatted for the profile's
        // country.
        // This local AU number fits the length of a US number, so it's
        // formatted for US.
        PhoneNumberFormatCase("02 9374 4000", "US", "+1 029-374-4000"),
        // This local AU number is formatted as an US number, even if it's
        // invlaid.
        PhoneNumberFormatCase("2 9374 4000", "US", "+1 293744000"),

        //////////////////////////
        // MX phone in MX.
        //////////////////////////
        // Formatted phone numbers.
        PhoneNumberFormatCase("+52 55 5342 8400", "MX", "+52 55 5342 8400"),
        PhoneNumberFormatCase("52 55 5342 8400", "MX", "+52 55 5342 8400"),
        PhoneNumberFormatCase("55 5342 8400", "MX", "+52 55 5342 8400"),
        // Raw phone numbers.
        PhoneNumberFormatCase("+525553428400", "MX", "+52 55 5342 8400"),
        PhoneNumberFormatCase("525553428400", "MX", "+52 55 5342 8400"),
        PhoneNumberFormatCase("5553428400", "MX", "+52 55 5342 8400"),

        //////////////////////////
        // MX phone in US.
        //////////////////////////
        // A MX phone with the country code is correctly formatted as a MX
        // number.
        PhoneNumberFormatCase("+52 55 5342 8400", "US", "+52 55 5342 8400"),
        PhoneNumberFormatCase("52 55 5342 8400", "US", "+52 55 5342 8400"),
        // This local MX number fits the length of a US number, so it's
        // formatted for US.
        PhoneNumberFormatCase("55 5342 8400", "US", "+1 555-342-8400")));

INSTANTIATE_TEST_CASE_P(
    GetFormattedPhoneNumberForDisplay_EdgeCases,
    GetFormattedPhoneNumberForDisplayTest,
    testing::Values(
        //////////////////////////
        // No country.
        //////////////////////////
        // Fallback to locale if no country is set.
        PhoneNumberFormatCase("52 55 5342 8400",
                              "",
                              "+52 55 5342 8400",
                              "es_MX"),
        PhoneNumberFormatCase("55 5342 8400", "", "+52 55 5342 8400", "es_MX"),
        PhoneNumberFormatCase("55 5342 8400", "", "+1 555-342-8400", "en_US"),
        PhoneNumberFormatCase("61 2 9374 4000", "", "+61 2 9374 4000", "en_AU"),
        PhoneNumberFormatCase("02 9374 4000", "", "+61 2 9374 4000", "en_AU"),

        //////////////////////////
        // No country or locale.
        //////////////////////////
        // Format according to the country code.
        PhoneNumberFormatCase("61 2 9374 4000", "", "+61 2 9374 4000"),
        PhoneNumberFormatCase("52 55 5342 8400", "", "+52 55 5342 8400"),
        PhoneNumberFormatCase("1 415 555 5555", "", "+1 415-555-5555"),
        // If no country code is found, formats for US.
        PhoneNumberFormatCase("02 9374 4000", "", "+1 029-374-4000"),
        PhoneNumberFormatCase("2 9374 4000", "", "+1 293744000"),
        PhoneNumberFormatCase("55 5342 8400", "", "+1 555-342-8400"),
        PhoneNumberFormatCase("52 55 5342 8400", "", "+52 55 5342 8400"),
        PhoneNumberFormatCase("415-555-5555", "", "+1 415-555-5555")));
}  // namespace data_util
}  // namespace payments
