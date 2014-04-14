// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/content/browser/wallet/wallet_address.h"
#include "components/autofill/content/browser/wallet/wallet_test_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

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
    "    \"dependent_locality_name\":\"dependent_locality_name\","
    "    \"administrative_area_name\":\"administrative_area_name\","
    "    \"postal_code_number\":\"postal_code_number\","
    "    \"sorting_code\":\"sorting_code\","
    "    \"country_name_code\":\"US\","
    "    \"language_code\":\"language_code\""
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
    "    \"dependent_locality_name\":\"dependent_locality_name\","
    "    \"administrative_area_name\":\"administrative_area_name\","
    "    \"postal_code_number\":\"postal_code_number\","
    "    \"sorting_code\":\"sorting_code\""
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
    "    \"dependent_locality_name\":\"dependent_locality_name\","
    "    \"administrative_area_name\":\"administrative_area_name\","
    "    \"postal_code_number\":\"postal_code_number\","
    "    \"sorting_code\":\"sorting_code\","
    "    \"country_name_code\":\"US\""
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
    "    \"dependent_locality_name\":\"dependent_locality_name\","
    "    \"administrative_area_name\":\"administrative_area_name\","
    "    \"sorting_code\":\"sorting_code\","
    "    \"country_name_code\":\"US\""
    "  }"
    "}";

const char kAddressMissingLanguageCode[] =
    "{"
    "  \"id\":\"id\","
    "  \"phone_number\":\"phone_number\","
    "  \"is_minimal_address\":true,"
    "  \"postal_address\":"
    "  {"
    "    \"recipient_name\":\"recipient_name\","
    "    \"address_line\":"
    "    ["
    "      \"address_line_1\","
    "      \"address_line_2\""
    "    ],"
    "    \"locality_name\":\"locality_name\","
    "    \"dependent_locality_name\":\"dependent_locality_name\","
    "    \"administrative_area_name\":\"administrative_area_name\","
    "    \"country_name_code\":\"US\","
    "    \"postal_code_number\":\"postal_code_number\","
    "    \"sorting_code\":\"sorting_code\""
    "  }"
    "}";

const char kValidAddress[] =
    "{"
    "  \"id\":\"id\","
    "  \"phone_number\":\"phone_number\","
    "  \"is_minimal_address\":true,"
    "  \"postal_address\":"
    "  {"
    "    \"recipient_name\":\"recipient_name\","
    "    \"address_line\":"
    "    ["
    "      \"address_line_1\","
    "      \"address_line_2\""
    "    ],"
    "    \"locality_name\":\"locality_name\","
    "    \"dependent_locality_name\":\"dependent_locality_name\","
    "    \"administrative_area_name\":\"administrative_area_name\","
    "    \"country_name_code\":\"US\","
    "    \"postal_code_number\":\"postal_code_number\","
    "    \"sorting_code\":\"sorting_code\","
    "    \"language_code\":\"language_code\""
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
  "  \"sorting_code\":\"sorting_code\","
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
  "  \"country_code\":\"US\""
  "}";

const char kClientAddressMissingName[] =
  "{"
  "  \"address1\":\"address1\","
  "  \"address2\":\"address2\","
  "  \"city\":\"city\","
  "  \"state\":\"state\","
  "  \"postal_code\":\"postal_code\","
  "  \"sorting_code\":\"sorting_code\","
  "  \"phone_number\":\"phone_number\","
  "  \"country_code\":\"US\""
  "}";

const char kClientAddressMissingLanguageCode[] =
  "{"
  "  \"name\":\"name\","
  "  \"address1\":\"address1\","
  "  \"address2\":\"address2\","
  "  \"city\":\"city\","
  "  \"dependent_locality_name\":\"district\","
  "  \"state\":\"state\","
  "  \"postal_code\":\"postal_code\","
  "  \"sorting_code\":\"sorting_code\","
  "  \"phone_number\":\"phone_number\","
  "  \"country_code\":\"US\","
  "  \"type\":\"FULL\""
  "}";

const char kClientValidAddress[] =
  "{"
  "  \"name\":\"name\","
  "  \"address1\":\"address1\","
  "  \"address2\":\"address2\","
  "  \"city\":\"city\","
  "  \"dependent_locality_name\":\"district\","
  "  \"state\":\"state\","
  "  \"postal_code\":\"postal_code\","
  "  \"sorting_code\":\"sorting_code\","
  "  \"phone_number\":\"phone_number\","
  "  \"country_code\":\"US\","
  "  \"type\":\"FULL\","
  "  \"language_code\":\"language_code\""
  "}";

}  // anonymous namespace

namespace autofill {
namespace wallet {

class WalletAddressTest : public testing::Test {
 public:
  WalletAddressTest() {}
 protected:
  void SetUpDictionary(const std::string& json) {
    scoped_ptr<base::Value> value(base::JSONReader::Read(json));
    DCHECK(value.get());
    DCHECK(value->IsType(base::Value::TYPE_DICTIONARY));
    dict_.reset(static_cast<base::DictionaryValue*>(value.release()));
  }

  scoped_ptr<const base::DictionaryValue> dict_;
};

TEST_F(WalletAddressTest, AddressEqualsIgnoreID) {
  Address address1("US",
                   ASCIIToUTF16("recipient_name"),
                   StreetAddress("address_line_1", "address_line_2"),
                   ASCIIToUTF16("locality_name"),
                   ASCIIToUTF16("dependent_locality_name"),
                   ASCIIToUTF16("administrative_area_name"),
                   ASCIIToUTF16("postal_code_number"),
                   ASCIIToUTF16("sorting_code"),
                   ASCIIToUTF16("phone_number"),
                   "id1",
                   "language_code");
  // Same as address1, only id is different.
  Address address2("US",
                   ASCIIToUTF16("recipient_name"),
                   StreetAddress("address_line_1", "address_line_2"),
                   ASCIIToUTF16("locality_name"),
                   ASCIIToUTF16("dependent_locality_name"),
                   ASCIIToUTF16("administrative_area_name"),
                   ASCIIToUTF16("postal_code_number"),
                   ASCIIToUTF16("sorting_code"),
                   ASCIIToUTF16("phone_number"),
                   "id2",
                   "language_code");
  // Has same id as address1, but name is different.
  Address address3("US",
                   ASCIIToUTF16("a_different_name"),
                   StreetAddress("address_line_1", "address_line_2"),
                   ASCIIToUTF16("locality_name"),
                   ASCIIToUTF16("dependent_locality_name"),
                   ASCIIToUTF16("administrative_area_name"),
                   ASCIIToUTF16("phone_number"),
                   ASCIIToUTF16("postal_code_number"),
                   ASCIIToUTF16("sorting_code"),
                   "id1",
                   "language_code");
  // Same as address1, but no id.
  Address address4("US",
                   ASCIIToUTF16("recipient_name"),
                   StreetAddress("address_line_1", "address_line_2"),
                   ASCIIToUTF16("locality_name"),
                   ASCIIToUTF16("dependent_locality_name"),
                   ASCIIToUTF16("administrative_area_name"),
                   ASCIIToUTF16("postal_code_number"),
                   ASCIIToUTF16("sorting_code"),
                   ASCIIToUTF16("phone_number"),
                   std::string(),
                   "language_code");
  // Same as address1, only language code is different.
  Address address5("US",
                   ASCIIToUTF16("recipient_name"),
                   StreetAddress("address_line_1", "address_line_2"),
                   ASCIIToUTF16("locality_name"),
                   ASCIIToUTF16("dependent_locality_name"),
                   ASCIIToUTF16("administrative_area_name"),
                   ASCIIToUTF16("postal_code_number"),
                   ASCIIToUTF16("sorting_code"),
                   ASCIIToUTF16("phone_number"),
                   "id1",
                   "other_language_code");

  // Compare the address has id field to itself.
  EXPECT_EQ(address1, address1);
  EXPECT_TRUE(address1.EqualsIgnoreID(address1));

  // Compare the address has no id field to itself
  EXPECT_EQ(address4, address4);
  EXPECT_TRUE(address4.EqualsIgnoreID(address4));

  // Compare two addresses with different id.
  EXPECT_NE(address1, address2);
  EXPECT_TRUE(address1.EqualsIgnoreID(address2));
  EXPECT_TRUE(address2.EqualsIgnoreID(address1));

  // Compare two different addresses.
  EXPECT_NE(address1, address3);
  EXPECT_FALSE(address1.EqualsIgnoreID(address3));
  EXPECT_FALSE(address3.EqualsIgnoreID(address1));

  // Compare two same addresses, one has id, the other doesn't.
  EXPECT_NE(address1, address4);
  EXPECT_TRUE(address1.EqualsIgnoreID(address4));
  EXPECT_TRUE(address4.EqualsIgnoreID(address1));

  // Compare two addresses with different language code.
  EXPECT_NE(address1, address5);
  EXPECT_TRUE(address1.EqualsIgnoreID(address5));
  EXPECT_TRUE(address5.EqualsIgnoreID(address1));
}

TEST_F(WalletAddressTest, CreateAddressMissingObjectId) {
  SetUpDictionary(kAddressMissingObjectId);
  Address address("US",
                  ASCIIToUTF16("recipient_name"),
                  StreetAddress("address_line_1", "address_line_2"),
                  ASCIIToUTF16("locality_name"),
                  ASCIIToUTF16("dependent_locality_name"),
                  ASCIIToUTF16("administrative_area_name"),
                  ASCIIToUTF16("postal_code_number"),
                  ASCIIToUTF16("sorting_code"),
                  ASCIIToUTF16("phone_number"),
                  std::string(),
                  "language_code");
  EXPECT_EQ(address, *Address::CreateAddress(*dict_));
}

TEST_F(WalletAddressTest, CreateAddressWithIDMissingObjectId) {
  SetUpDictionary(kAddressMissingObjectId);
  EXPECT_EQ(NULL, Address::CreateAddressWithID(*dict_).get());
}

TEST_F(WalletAddressTest, CreateAddressMissingCountryNameCode) {
  SetUpDictionary(kAddressMissingCountryNameCode);
  EXPECT_EQ(NULL, Address::CreateAddress(*dict_).get());
  EXPECT_EQ(NULL, Address::CreateAddressWithID(*dict_).get());
}

TEST_F(WalletAddressTest, CreateAddressMissingRecipientName) {
  SetUpDictionary(kAddressMissingRecipientName);
  EXPECT_EQ(NULL, Address::CreateAddress(*dict_).get());
  EXPECT_EQ(NULL, Address::CreateAddressWithID(*dict_).get());
}

TEST_F(WalletAddressTest, CreateAddressMissingPostalCodeNumber) {
  SetUpDictionary(kAddressMissingPostalCodeNumber);
  EXPECT_EQ(NULL, Address::CreateAddress(*dict_).get());
  EXPECT_EQ(NULL, Address::CreateAddressWithID(*dict_).get());
}

TEST_F(WalletAddressTest, CreateAddressMissingLanguageCode) {
  SetUpDictionary(kAddressMissingLanguageCode);
  Address address("US",
                  ASCIIToUTF16("recipient_name"),
                  StreetAddress("address_line_1", "address_line_2"),
                  ASCIIToUTF16("locality_name"),
                  ASCIIToUTF16("dependent_locality_name"),
                  ASCIIToUTF16("administrative_area_name"),
                  ASCIIToUTF16("postal_code_number"),
                  ASCIIToUTF16("sorting_code"),
                  ASCIIToUTF16("phone_number"),
                  "id",
                  std::string());
  address.set_is_complete_address(false);
  EXPECT_EQ(address, *Address::CreateAddress(*dict_));
  EXPECT_EQ(address, *Address::CreateAddressWithID(*dict_));
}

TEST_F(WalletAddressTest, CreateAddressWithID) {
  SetUpDictionary(kValidAddress);
  Address address("US",
                  ASCIIToUTF16("recipient_name"),
                  StreetAddress("address_line_1", "address_line_2"),
                  ASCIIToUTF16("locality_name"),
                  ASCIIToUTF16("dependent_locality_name"),
                  ASCIIToUTF16("administrative_area_name"),
                  ASCIIToUTF16("postal_code_number"),
                  ASCIIToUTF16("sorting_code"),
                  ASCIIToUTF16("phone_number"),
                  "id",
                  "language_code");
  address.set_is_complete_address(false);
  EXPECT_EQ(address, *Address::CreateAddress(*dict_));
  EXPECT_EQ(address, *Address::CreateAddressWithID(*dict_));
}

TEST_F(WalletAddressTest, CreateDisplayAddressMissingCountryNameCode) {
  SetUpDictionary(kClientAddressMissingCountryCode);
  EXPECT_EQ(NULL, Address::CreateDisplayAddress(*dict_).get());
}

TEST_F(WalletAddressTest, CreateDisplayAddressMissingName) {
  SetUpDictionary(kClientAddressMissingName);
  EXPECT_EQ(NULL, Address::CreateDisplayAddress(*dict_).get());
}

TEST_F(WalletAddressTest, CreateDisplayAddressMissingPostalCode) {
  SetUpDictionary(kClientAddressMissingPostalCode);
  EXPECT_EQ(NULL, Address::CreateDisplayAddress(*dict_).get());
}

TEST_F(WalletAddressTest, CreateDisplayAddressMissingLanguageCode) {
  SetUpDictionary(kClientAddressMissingLanguageCode);
  Address address("US",
                  ASCIIToUTF16("name"),
                  StreetAddress("address1", "address2"),
                  ASCIIToUTF16("city"),
                  ASCIIToUTF16("district"),
                  ASCIIToUTF16("state"),
                  ASCIIToUTF16("postal_code"),
                  ASCIIToUTF16("sorting_code"),
                  ASCIIToUTF16("phone_number"),
                  std::string(),
                  std::string());
  EXPECT_EQ(address, *Address::CreateDisplayAddress(*dict_));
}

TEST_F(WalletAddressTest, CreateDisplayAddress) {
  SetUpDictionary(kClientValidAddress);
  Address address("US",
                  ASCIIToUTF16("name"),
                  StreetAddress("address1", "address2"),
                  ASCIIToUTF16("city"),
                  ASCIIToUTF16("district"),
                  ASCIIToUTF16("state"),
                  ASCIIToUTF16("postal_code"),
                  ASCIIToUTF16("sorting_code"),
                  ASCIIToUTF16("phone_number"),
                  std::string(),
                  "language_code");
  EXPECT_EQ(address, *Address::CreateDisplayAddress(*dict_));
}

TEST_F(WalletAddressTest, ToDictionaryWithoutID) {
  base::DictionaryValue expected;
  expected.SetString("country_name_code",
                     "US");
  expected.SetString("recipient_name",
                     "recipient_name");
  expected.SetString("locality_name",
                     "locality_name");
  expected.SetString("dependent_locality_name",
                     "dependent_locality_name");
  expected.SetString("administrative_area_name",
                     "administrative_area_name");
  expected.SetString("postal_code_number",
                     "postal_code_number");
  expected.SetString("sorting_code",
                     "sorting_code");
  expected.SetString("language_code",
                     "language_code");
  base::ListValue* address_lines = new base::ListValue();
  address_lines->AppendString("address_line_1");
  address_lines->AppendString("address_line_2");
  expected.Set("address_line", address_lines);

  Address address("US",
                  ASCIIToUTF16("recipient_name"),
                  StreetAddress("address_line_1", "address_line_2"),
                  ASCIIToUTF16("locality_name"),
                  ASCIIToUTF16("dependent_locality_name"),
                  ASCIIToUTF16("administrative_area_name"),
                  ASCIIToUTF16("postal_code_number"),
                  ASCIIToUTF16("sorting_code"),
                  ASCIIToUTF16("phone_number"),
                  std::string(),
                  "language_code");

  EXPECT_TRUE(expected.Equals(address.ToDictionaryWithoutID().get()));
}

TEST_F(WalletAddressTest, ToDictionaryWithID) {
  base::DictionaryValue expected;
  expected.SetString("id", "id");
  expected.SetString("phone_number", "phone_number");
  expected.SetString("postal_address.country_name_code",
                     "US");
  expected.SetString("postal_address.recipient_name",
                     "recipient_name");
  expected.SetString("postal_address.locality_name",
                     "locality_name");
  expected.SetString("postal_address.dependent_locality_name",
                     "dependent_locality_name");
  expected.SetString("postal_address.administrative_area_name",
                     "administrative_area_name");
  expected.SetString("postal_address.postal_code_number",
                     "postal_code_number");
  expected.SetString("postal_address.sorting_code",
                     "sorting_code");
  expected.SetString("postal_address.language_code",
                     "language_code");
  base::ListValue* address_lines = new base::ListValue();
  address_lines->AppendString("address_line_1");
  address_lines->AppendString("address_line_2");
  expected.Set("postal_address.address_line", address_lines);

  Address address("US",
                  ASCIIToUTF16("recipient_name"),
                  StreetAddress("address_line_1", "address_line_2"),
                  ASCIIToUTF16("locality_name"),
                  ASCIIToUTF16("dependent_locality_name"),
                  ASCIIToUTF16("administrative_area_name"),
                  ASCIIToUTF16("postal_code_number"),
                  ASCIIToUTF16("sorting_code"),
                  ASCIIToUTF16("phone_number"),
                  "id",
                  "language_code");

  EXPECT_TRUE(expected.Equals(address.ToDictionaryWithID().get()));
}

// Verifies that WalletAddress::GetInfo() can correctly return both country
// codes and localized country names.
TEST_F(WalletAddressTest, GetCountryInfo) {
  Address address("FR",
                  ASCIIToUTF16("recipient_name"),
                  StreetAddress("address_line_1", "address_line_2"),
                  ASCIIToUTF16("locality_name"),
                  ASCIIToUTF16("dependent_locality_name"),
                  ASCIIToUTF16("administrative_area_name"),
                  ASCIIToUTF16("postal_code_number"),
                  ASCIIToUTF16("sorting_code"),
                  ASCIIToUTF16("phone_number"),
                  "id1",
                  "language_code");

  AutofillType type = AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE);
  EXPECT_EQ(ASCIIToUTF16("FR"), address.GetInfo(type, "en-US"));

  type = AutofillType(HTML_TYPE_COUNTRY_NAME, HTML_MODE_NONE);
  EXPECT_EQ(ASCIIToUTF16("France"), address.GetInfo(type, "en-US"));

  type = AutofillType(ADDRESS_HOME_COUNTRY);
  EXPECT_EQ(ASCIIToUTF16("France"), address.GetInfo(type, "en-US"));
}

// Verifies that WalletAddress::GetInfo() can correctly return a concatenated
// full street address.
TEST_F(WalletAddressTest, GetStreetAddress) {
  std::vector<base::string16> street_address = StreetAddress(
      "address_line_1", "address_line_2");
  // Address has both lines 1 and 2.
  Address address1("FR",
                   ASCIIToUTF16("recipient_name"),
                   street_address,
                   ASCIIToUTF16("locality_name"),
                   ASCIIToUTF16("dependent_locality_name"),
                   ASCIIToUTF16("administrative_area_name"),
                   ASCIIToUTF16("postal_code_number"),
                   ASCIIToUTF16("sorting_code"),
                   ASCIIToUTF16("phone_number"),
                   "id1",
                   "language_code");
  AutofillType type = AutofillType(HTML_TYPE_STREET_ADDRESS, HTML_MODE_NONE);
  EXPECT_EQ(ASCIIToUTF16("address_line_1\naddress_line_2"),
            address1.GetInfo(type, "en-US"));

  // Address has only line 1.
  street_address.resize(1);
  Address address2("FR",
                   ASCIIToUTF16("recipient_name"),
                   street_address,
                   ASCIIToUTF16("locality_name"),
                   ASCIIToUTF16("dependent_locality_name"),
                   ASCIIToUTF16("administrative_area_name"),
                   ASCIIToUTF16("postal_code_number"),
                   ASCIIToUTF16("sorting_code"),
                   ASCIIToUTF16("phone_number"),
                   "id1",
                   "language_code");
  EXPECT_EQ(ASCIIToUTF16("address_line_1"), address2.GetInfo(type, "en-US"));

  // Address has no address lines.
  street_address.clear();
  Address address3("FR",
                   ASCIIToUTF16("recipient_name"),
                   street_address,
                   ASCIIToUTF16("locality_name"),
                   ASCIIToUTF16("dependent_locality_name"),
                   ASCIIToUTF16("administrative_area_name"),
                   ASCIIToUTF16("postal_code_number"),
                   ASCIIToUTF16("sorting_code"),
                   ASCIIToUTF16("phone_number"),
                   "id1",
                   "language_code");
  EXPECT_EQ(base::string16(), address3.GetInfo(type, "en-US"));
}

}  // namespace wallet
}  // namespace autofill
