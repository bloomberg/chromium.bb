// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/wallet/wallet_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAddressMissingObjectId[] =
    "{"
    "  \"phone_number\":\"phone_number\","
    "  \"postal_address\":"
    "  {"
    "    \"recipient_name\":\"recipient_name\","
    "    \"address_line\":"
    "    ["
    "      \"address_line_1\","
    "      \"address_line_2\""
    "    ],"
    "    \"locality_name\":\"locality_name\","
    "    \"administrative_area_name\":\"administrative_area_name\","
    "    \"postal_code_number\":\"postal_code_number\","
    "    \"country_name_code\":\"country_name_code\""
    "  }"
    "}";

const char kAddressMissingCountryNameCode[] =
    "{"
    "  \"id\":\"id\","
    "  \"phone_number\":\"phone_number\","
    "  \"postal_address\":"
    "  {"
    "    \"recipient_name\":\"recipient_name\","
    "    \"address_line\":"
    "    ["
    "      \"address_line_1\","
    "      \"address_line_2\""
    "    ],"
    "    \"locality_name\":\"locality_name\","
    "    \"administrative_area_name\":\"administrative_area_name\","
    "    \"postal_code_number\":\"postal_code_number\""
    "  }"
    "}";

const char kAddressMissingRecipientName[] =
    "{"
    "  \"id\":\"id\","
    "  \"phone_number\":\"phone_number\","
    "  \"postal_address\":"
    "  {"
    "    \"address_line\":"
    "    ["
    "      \"address_line_1\","
    "      \"address_line_2\""
    "    ],"
    "    \"locality_name\":\"locality_name\","
    "    \"administrative_area_name\":\"administrative_area_name\","
    "    \"postal_code_number\":\"postal_code_number\","
    "    \"country_name_code\":\"country_name_code\""
    "  }"
    "}";

const char kAddressMissingPostalCodeNumber[] =
    "{"
    "  \"id\":\"id\","
    "  \"phone_number\":\"phone_number\","
    "  \"postal_address\":"
    "  {"
    "    \"recipient_name\":\"recipient_name\","
    "    \"address_line\":"
    "    ["
    "      \"address_line_1\","
    "      \"address_line_2\""
    "    ],"
    "    \"locality_name\":\"locality_name\","
    "    \"administrative_area_name\":\"administrative_area_name\","
    "    \"country_name_code\":\"country_name_code\""
    "  }"
    "}";

const char kValidAddress[] =
    "{"
    "  \"id\":\"id\","
    "  \"phone_number\":\"phone_number\","
    "  \"postal_address\":"
    "  {"
    "    \"recipient_name\":\"recipient_name\","
    "    \"address_line\":"
    "    ["
    "      \"address_line_1\","
    "      \"address_line_2\""
    "    ],"
    "    \"locality_name\":\"locality_name\","
    "    \"administrative_area_name\":\"administrative_area_name\","
    "    \"country_name_code\":\"country_name_code\","
    "    \"postal_code_number\":\"postal_code_number\""
    "  }"
    "}";

const char kClientAddressMissingCountryCode[] =
  "{"
  "  \"name\":\"name\","
  "  \"address1\":\"address1\","
  "  \"address2\":\"address2\","
  "  \"city\":\"city\","
  "  \"state\":\"state\","
  "  \"postal_code\":\"postal_code\","
  "  \"phone_number\":\"phone_number\""
  "}";

const char kClientAddressMissingPostalCode[] =
  "{"
  "  \"name\":\"name\","
  "  \"address1\":\"address1\","
  "  \"address2\":\"address2\","
  "  \"city\":\"city\","
  "  \"state\":\"state\","
  "  \"phone_number\":\"phone_number\","
  "  \"country_code\":\"country_code\""
  "}";

const char kClientAddressMissingName[] =
  "{"
  "  \"address1\":\"address1\","
  "  \"address2\":\"address2\","
  "  \"city\":\"city\","
  "  \"state\":\"state\","
  "  \"postal_code\":\"postal_code\","
  "  \"phone_number\":\"phone_number\","
  "  \"country_code\":\"country_code\""
  "}";

const char kClientValidAddress[] =
  "{"
  "  \"name\":\"name\","
  "  \"address1\":\"address1\","
  "  \"address2\":\"address2\","
  "  \"city\":\"city\","
  "  \"state\":\"state\","
  "  \"postal_code\":\"postal_code\","
  "  \"phone_number\":\"phone_number\","
  "  \"country_code\":\"country_code\""
  "}";

}  // anonymous namespace

namespace wallet {

class WalletAddressTest : public testing::Test {
 public:
  WalletAddressTest() {}
 protected:
  void SetUpDictionary(const std::string& json) {
    scoped_ptr<Value> value(base::JSONReader::Read(json));
    DCHECK(value.get());
    DCHECK(value->IsType(Value::TYPE_DICTIONARY));
    dict_.reset(static_cast<DictionaryValue*>(value.release()));
  }
  scoped_ptr<const DictionaryValue> dict_;
};

TEST_F(WalletAddressTest, CreateAddressWithIDMissingObjectId) {
  SetUpDictionary(kAddressMissingObjectId);
  ASSERT_EQ(NULL, Address::CreateAddressWithID(*dict_).get());
}

TEST_F(WalletAddressTest, CreateAddressWithIDMissingCountryNameCode) {
  SetUpDictionary(kAddressMissingCountryNameCode);
  ASSERT_EQ(NULL, Address::CreateAddressWithID(*dict_).get());
}

TEST_F(WalletAddressTest, CreateAddressWithIDMissingRecipientName) {
  SetUpDictionary(kAddressMissingRecipientName);
  ASSERT_EQ(NULL, Address::CreateAddressWithID(*dict_).get());
}

TEST_F(WalletAddressTest, CreateAddressWithIDMissingPostalCodeNumber) {
  SetUpDictionary(kAddressMissingPostalCodeNumber);
  ASSERT_EQ(NULL, Address::CreateAddressWithID(*dict_).get());
}

TEST_F(WalletAddressTest, CreateAddressWithID) {
  SetUpDictionary(kValidAddress);
  Address address("country_name_code",
                  ASCIIToUTF16("recipient_name"),
                  ASCIIToUTF16("address_line_1"),
                  ASCIIToUTF16("address_line_2"),
                  ASCIIToUTF16("locality_name"),
                  ASCIIToUTF16("administrative_area_name"),
                  ASCIIToUTF16("postal_code_number"),
                  ASCIIToUTF16("phone_number"),
                  "id");
  ASSERT_EQ(address, *Address::CreateAddressWithID(*dict_));
}

TEST_F(WalletAddressTest, CreateDisplayAddressMissingCountryNameCode) {
  SetUpDictionary(kClientAddressMissingCountryCode);
  ASSERT_EQ(NULL, Address::CreateDisplayAddress(*dict_).get());
}

TEST_F(WalletAddressTest, CreateDisplayAddressMissingName) {
  SetUpDictionary(kClientAddressMissingName);
  ASSERT_EQ(NULL, Address::CreateDisplayAddress(*dict_).get());
}

TEST_F(WalletAddressTest, CreateDisplayAddressMissingPostalCode) {
  SetUpDictionary(kClientAddressMissingPostalCode);
  ASSERT_EQ(NULL, Address::CreateDisplayAddress(*dict_).get());
}

TEST_F(WalletAddressTest, CreateDisplayAddress) {
  SetUpDictionary(kClientValidAddress);
  Address address("country_code",
                  ASCIIToUTF16("name"),
                  ASCIIToUTF16("address1"),
                  ASCIIToUTF16("address2"),
                  ASCIIToUTF16("city"),
                  ASCIIToUTF16("state"),
                  ASCIIToUTF16("postal_code"),
                  ASCIIToUTF16("phone_number"),
                  "");
  ASSERT_EQ(address, *Address::CreateDisplayAddress(*dict_));
}

TEST_F(WalletAddressTest, ToDictionaryWithoutID) {
  base::DictionaryValue expected;
  expected.SetString("country_name_code",
                     "country_name_code");
  expected.SetString("recipient_name",
                     "recipient_name");
  expected.SetString("locality_name",
                     "locality_name");
  expected.SetString("administrative_area_name",
                     "administrative_area_name");
  expected.SetString("postal_code_number",
                     "postal_code_number");
  base::ListValue* address_lines = new base::ListValue();
  address_lines->AppendString("address_line_1");
  address_lines->AppendString("address_line_2");
  expected.Set("address_line", address_lines);

  Address address("country_name_code",
                  ASCIIToUTF16("recipient_name"),
                  ASCIIToUTF16("address_line_1"),
                  ASCIIToUTF16("address_line_2"),
                  ASCIIToUTF16("locality_name"),
                  ASCIIToUTF16("administrative_area_name"),
                  ASCIIToUTF16("postal_code_number"),
                  ASCIIToUTF16("phone_number"),
                  "");

  EXPECT_TRUE(expected.Equals(address.ToDictionaryWithoutID().get()));
}

TEST_F(WalletAddressTest, ToDictionaryWithID) {
  base::DictionaryValue expected;
  expected.SetString("id", "id");
  expected.SetString("phone_number", "phone_number");
  expected.SetString("postal_address.country_name_code",
                     "country_name_code");
  expected.SetString("postal_address.recipient_name",
                     "recipient_name");
  expected.SetString("postal_address.locality_name",
                     "locality_name");
  expected.SetString("postal_address.administrative_area_name",
                     "administrative_area_name");
  expected.SetString("postal_address.postal_code_number",
                     "postal_code_number");
  base::ListValue* address_lines = new base::ListValue();
  address_lines->AppendString("address_line_1");
  address_lines->AppendString("address_line_2");
  expected.Set("postal_address.address_line", address_lines);

  Address address("country_name_code",
                  ASCIIToUTF16("recipient_name"),
                  ASCIIToUTF16("address_line_1"),
                  ASCIIToUTF16("address_line_2"),
                  ASCIIToUTF16("locality_name"),
                  ASCIIToUTF16("administrative_area_name"),
                  ASCIIToUTF16("postal_code_number"),
                  ASCIIToUTF16("phone_number"),
                  "id");

  EXPECT_TRUE(expected.Equals(address.ToDictionaryWithID().get()));
}

}  // namespace wallet

