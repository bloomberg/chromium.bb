// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/autofill/wallet/full_wallet.h"
#include "chrome/browser/autofill/wallet/required_action.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kFullWalletValidResponse[] =
    "{"
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"iin\":\"iin\","
    "  \"rest\":\"rest\","
    "  \"billing_address\":"
    "  {"
    "    \"id\":\"id\","
    "    \"phone_number\":\"phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"address_line_1\","
    "        \"address_line_2\""
    "      ],"
    "      \"locality_name\":\"locality_name\","
    "      \"administrative_area_name\":\"administrative_area_name\","
    "      \"postal_code_number\":\"postal_code_number\","
    "      \"country_name_code\":\"country_name_code\""
    "    }"
    "  },"
    "  \"shipping_address\":"
    "  {"
    "    \"id\":\"ship_id\","
    "    \"phone_number\":\"ship_phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"ship_recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"ship_address_line_1\","
    "        \"ship_address_line_2\""
    "      ],"
    "      \"locality_name\":\"ship_locality_name\","
    "      \"administrative_area_name\":\"ship_admin_area_name\","
    "      \"postal_code_number\":\"ship_postal_code_number\","
    "      \"country_name_code\":\"ship_country_name_code\""
    "    }"
    "  },"
    "  \"required_action\":"
    "  ["
    "  ]"
    "}";

const char kFullWalletMissingExpirationMonth[] =
    "{"
    "  \"expiration_year\":2012,"
    "  \"iin\":\"iin\","
    "  \"rest\":\"rest\","
    "  \"billing_address\":"
    "  {"
    "    \"id\":\"id\","
    "    \"phone_number\":\"phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"address_line_1\","
    "        \"address_line_2\""
    "      ],"
    "      \"locality_name\":\"locality_name\","
    "      \"administrative_area_name\":\"administrative_area_name\","
    "      \"postal_code_number\":\"postal_code_number\","
    "      \"country_name_code\":\"country_name_code\""
    "    }"
    "  },"
    "  \"shipping_address\":"
    "  {"
    "    \"id\":\"ship_id\","
    "    \"phone_number\":\"ship_phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"ship_recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"ship_address_line_1\","
    "        \"ship_address_line_2\""
    "      ],"
    "      \"locality_name\":\"ship_locality_name\","
    "      \"administrative_area_name\":\"ship_admin_area_name\","
    "      \"postal_code_number\":\"ship_postal_code_number\","
    "      \"country_name_code\":\"ship_country_name_code\""
    "    }"
    "  },"
    "  \"required_action\":"
    "  ["
    "  ]"
    "}";

const char kFullWalletMissingExpirationYear[] =
    "{"
    "  \"expiration_month\":12,"
    "  \"iin\":\"iin\","
    "  \"rest\":\"rest\","
    "  \"billing_address\":"
    "  {"
    "    \"id\":\"id\","
    "    \"phone_number\":\"phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"address_line_1\","
    "        \"address_line_2\""
    "      ],"
    "      \"locality_name\":\"locality_name\","
    "      \"administrative_area_name\":\"administrative_area_name\","
    "      \"postal_code_number\":\"postal_code_number\","
    "      \"country_name_code\":\"country_name_code\""
    "    }"
    "  },"
    "  \"shipping_address\":"
    "  {"
    "    \"id\":\"ship_id\","
    "    \"phone_number\":\"ship_phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"ship_recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"ship_address_line_1\","
    "        \"ship_address_line_2\""
    "      ],"
    "      \"locality_name\":\"ship_locality_name\","
    "      \"administrative_area_name\":\"ship_admin_area_name\","
    "      \"postal_code_number\":\"ship_postal_code_number\","
    "      \"country_name_code\":\"ship_country_name_code\""
    "    }"
    "  },"
    "  \"required_action\":"
    "  ["
    "  ]"
    "}";

const char kFullWalletMissingIin[] =
    "{"
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"rest\":\"rest\","
    "  \"billing_address\":"
    "  {"
    "    \"id\":\"id\","
    "    \"phone_number\":\"phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"address_line_1\","
    "        \"address_line_2\""
    "      ],"
    "      \"locality_name\":\"locality_name\","
    "      \"administrative_area_name\":\"administrative_area_name\","
    "      \"postal_code_number\":\"postal_code_number\","
    "      \"country_name_code\":\"country_name_code\""
    "    }"
    "  },"
    "  \"shipping_address\":"
    "  {"
    "    \"id\":\"ship_id\","
    "    \"phone_number\":\"ship_phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"ship_recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"ship_address_line_1\","
    "        \"ship_address_line_2\""
    "      ],"
    "      \"locality_name\":\"ship_locality_name\","
    "      \"administrative_area_name\":\"ship_admin_area_name\","
    "      \"postal_code_number\":\"ship_postal_code_number\","
    "      \"country_name_code\":\"ship_country_name_code\""
    "    }"
    "  },"
    "  \"required_action\":"
    "  ["
    "  ]"
    "}";

const char kFullWalletMissingRest[] =
    "{"
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"iin\":\"iin\","
    "  \"billing_address\":"
    "  {"
    "    \"id\":\"id\","
    "    \"phone_number\":\"phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"address_line_1\","
    "        \"address_line_2\""
    "      ],"
    "      \"locality_name\":\"locality_name\","
    "      \"administrative_area_name\":\"administrative_area_name\","
    "      \"postal_code_number\":\"postal_code_number\","
    "      \"country_name_code\":\"country_name_code\""
    "    }"
    "  },"
    "  \"shipping_address\":"
    "  {"
    "    \"id\":\"ship_id\","
    "    \"phone_number\":\"ship_phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"ship_recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"ship_address_line_1\","
    "        \"ship_address_line_2\""
    "      ],"
    "      \"locality_name\":\"ship_locality_name\","
    "      \"administrative_area_name\":\"ship_admin_area_name\","
    "      \"postal_code_number\":\"ship_postal_code_number\","
    "      \"country_name_code\":\"ship_country_name_code\""
    "    }"
    "  },"
    "  \"required_action\":"
    "  ["
    "  ]"
    "}";

const char kFullWalletMissingBillingAddress[] =
    "{"
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"iin\":\"iin\","
    "  \"rest\":\"rest\","
    "  \"shipping_address\":"
    "  {"
    "    \"id\":\"ship_id\","
    "    \"phone_number\":\"ship_phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"ship_recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"ship_address_line_1\","
    "        \"ship_address_line_2\""
    "      ],"
    "      \"locality_name\":\"ship_locality_name\","
    "      \"administrative_area_name\":\"ship_admin_area_name\","
    "      \"postal_code_number\":\"ship_postal_code_number\","
    "      \"country_name_code\":\"ship_country_name_code\""
    "    }"
    "  },"
    "  \"required_action\":"
    "  ["
    "  ]"
    "}";

const char kFullWalletWithRequiredActions[] =
    "{"
    "  \"required_action\":"
    "  ["
    "    \"UPGRADE_MIN_ADDRESS\","
    "    \"update_EXPIRATION_date\","
    "    \"INVALID_form_field\","
    "    \"cvc_risk_CHALLENGE\""
    "  ]"
    "}";

const char kFullWalletWithInvalidRequiredActions[] =
    "{"
    "  \"required_action\":"
    "  ["
    "    \"  setup_wallet\","
    "    \"AcCePt_ToS  \","
    "    \"   \\tGAIA_auth   \\n\\r\","
    "    \" 忍者の正体 \""
    "  ]"
    "}";

const char kFullWalletMalformedBillingAddress[] =
    "{"
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"iin\":\"iin\","
    "  \"rest\":\"rest\","
    "  \"billing_address\":"
    "  {"
    "    \"id\":\"id\","
    "    \"phone_number\":\"phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"address_line_1\","
    "        \"address_line_2\""
    "      ],"
    "      \"locality_name\":\"locality_name\","
    "      \"administrative_area_name\":\"administrative_area_name\""
    "    }"
    "  },"
    "  \"shipping_address\":"
    "  {"
    "    \"id\":\"ship_id\","
    "    \"phone_number\":\"ship_phone_number\","
    "    \"postal_address\":"
    "    {"
    "      \"recipient_name\":\"ship_recipient_name\","
    "      \"address_line\":"
    "      ["
    "        \"ship_address_line_1\","
    "        \"ship_address_line_2\""
    "      ],"
    "      \"locality_name\":\"ship_locality_name\","
    "      \"administrative_area_name\":\"ship_admin_area_name\","
    "      \"postal_code_number\":\"ship_postal_code_number\","
    "      \"country_name_code\":\"ship_country_name_code\""
    "    }"
    "  },"
    "  \"required_action\":"
    "  ["
    "  ]"
    "}";

}  // anonymous namespace

namespace wallet {

class FullWalletTest : public testing::Test {
 public:
  FullWalletTest() {}
 protected:
  void SetUpDictionary(const std::string& json) {
    scoped_ptr<Value> value(base::JSONReader::Read(json));
    DCHECK(value.get());
    DCHECK(value->IsType(Value::TYPE_DICTIONARY));
    dict.reset(static_cast<DictionaryValue*>(value.release()));
  }
  scoped_ptr<DictionaryValue> dict;
};

TEST_F(FullWalletTest, CreateFullWalletMissingExpirationMonth) {
  SetUpDictionary(kFullWalletMissingExpirationMonth);
  ASSERT_EQ(NULL, FullWallet::CreateFullWallet(*dict).get());
}

TEST_F(FullWalletTest, CreateFullWalletMissingExpirationYear) {
  SetUpDictionary(kFullWalletMissingExpirationYear);
  ASSERT_EQ(NULL, FullWallet::CreateFullWallet(*dict).get());
}

TEST_F(FullWalletTest, CreateFullWalletMissingIin) {
  SetUpDictionary(kFullWalletMissingIin);
  ASSERT_EQ(NULL, FullWallet::CreateFullWallet(*dict).get());
}

TEST_F(FullWalletTest, CreateFullWalletMissingRest) {
  SetUpDictionary(kFullWalletMissingRest);
  ASSERT_EQ(NULL, FullWallet::CreateFullWallet(*dict).get());
}

TEST_F(FullWalletTest, CreateFullWalletMissingBillingAddress) {
  SetUpDictionary(kFullWalletMissingBillingAddress);
  ASSERT_EQ(NULL, FullWallet::CreateFullWallet(*dict).get());
}

TEST_F(FullWalletTest, CreateFullWalletMalformedBillingAddress) {
  SetUpDictionary(kFullWalletMalformedBillingAddress);
  ASSERT_EQ(NULL, FullWallet::CreateFullWallet(*dict).get());
}

TEST_F(FullWalletTest, CreateFullWalletWithRequiredActions) {
  SetUpDictionary(kFullWalletWithRequiredActions);

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(UPGRADE_MIN_ADDRESS);
  required_actions.push_back(UPDATE_EXPIRATION_DATE);
  required_actions.push_back(INVALID_FORM_FIELD);
  required_actions.push_back(CVC_RISK_CHALLENGE);

  FullWallet full_wallet(-1,
                         -1,
                         "",
                         "",
                         scoped_ptr<Address>(),
                         scoped_ptr<Address>(),
                         required_actions);
  ASSERT_EQ(full_wallet, *FullWallet::CreateFullWallet(*dict));

  DCHECK(!required_actions.empty());
  required_actions.pop_back();
  FullWallet different_required_actions(
      -1,
      -1,
      "",
      "",
      scoped_ptr<Address>(),
      scoped_ptr<Address>(),
      required_actions);
  ASSERT_NE(full_wallet, different_required_actions);
}

TEST_F(FullWalletTest, CreateFullWalletWithInvalidRequiredActions) {
  SetUpDictionary(kFullWalletWithInvalidRequiredActions);
  ASSERT_EQ(NULL, FullWallet::CreateFullWallet(*dict).get());
}

TEST_F(FullWalletTest, CreateFullWallet) {
  SetUpDictionary(kFullWalletValidResponse);
  scoped_ptr<Address> billing_address(new Address("country_name_code",
                                                  "recipient_name",
                                                  "address_line_1",
                                                  "address_line_2",
                                                  "locality_name",
                                                  "administrative_area_name",
                                                  "postal_code_number",
                                                  "phone_number",
                                                  "id"));
  scoped_ptr<Address> shipping_address(new Address("ship_country_name_code",
                                                   "ship_recipient_name",
                                                   "ship_address_line_1",
                                                   "ship_address_line_2",
                                                   "ship_locality_name",
                                                   "ship_admin_area_name",
                                                   "ship_postal_code_number",
                                                   "ship_phone_number",
                                                   "ship_id"));
  std::vector<RequiredAction> required_actions;
  FullWallet full_wallet(12,
                         2012,
                         "iin",
                         "rest",
                         billing_address.Pass(),
                         shipping_address.Pass(),
                         required_actions);
  ASSERT_EQ(full_wallet, *FullWallet::CreateFullWallet(*dict));
}

}  // namespace wallet

