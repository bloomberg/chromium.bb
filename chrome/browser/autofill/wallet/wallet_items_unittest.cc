// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/autofill/wallet/required_action.h"
#include "chrome/browser/autofill/wallet/wallet_items.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kMaskedInstrument[] =
    "{"
    "  \"descriptive_name\":\"descriptive_name\","
    "  \"type\":\"VISA\","
    "  \"supported_currency\":"
    "  ["
    "    \"currency\""
    "  ],"
    "  \"last_four_digits\":\"last_four_digits\","
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"brand\":\"brand\","
    "  \"billing_address\":"
    "  {"
    "    \"name\":\"name\","
    "    \"address1\":\"address1\","
    "    \"address2\":\"address2\","
    "    \"city\":\"city\","
    "    \"state\":\"state\","
    "    \"postal_code\":\"postal_code\","
    "    \"phone_number\":\"phone_number\","
    "    \"country_code\":\"country_code\""
    "  },"
    "  \"status\":\"VALID\","
    "  \"object_id\":\"object_id\""
    "}";

const char kMaskedInstrumentMissingStatus[] =
    "{"
    "  \"descriptive_name\":\"descriptive_name\","
    "  \"type\":\"VISA\","
    "  \"supported_currency\":"
    "  ["
    "    \"currency\""
    "  ],"
    "  \"last_four_digits\":\"last_four_digits\","
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"brand\":\"brand\","
    "  \"billing_address\":"
    "  {"
    "    \"name\":\"name\","
    "    \"address1\":\"address1\","
    "    \"address2\":\"address2\","
    "    \"city\":\"city\","
    "    \"state\":\"state\","
    "    \"postal_code\":\"postal_code\","
    "    \"phone_number\":\"phone_number\","
    "    \"country_code\":\"country_code\""
    "  },"
    "  \"object_id\":\"object_id\""
    "}";

const char kMaskedInstrumentMissingType[] =
    "{"
    "  \"descriptive_name\":\"descriptive_name\","
    "  \"supported_currency\":"
    "  ["
    "    \"currency\""
    "  ],"
    "  \"last_four_digits\":\"last_four_digits\","
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"brand\":\"brand\","
    "  \"billing_address\":"
    "  {"
    "    \"name\":\"name\","
    "    \"address1\":\"address1\","
    "    \"address2\":\"address2\","
    "    \"city\":\"city\","
    "    \"state\":\"state\","
    "    \"postal_code\":\"postal_code\","
    "    \"phone_number\":\"phone_number\","
    "    \"country_code\":\"country_code\""
    "  },"
    "  \"status\":\"VALID\","
    "  \"object_id\":\"object_id\""
    "}";

const char kMaskedInstrumentMissingLastFourDigits[] =
    "{"
    "  \"descriptive_name\":\"descriptive_name\","
    "  \"type\":\"VISA\","
    "  \"supported_currency\":"
    "  ["
    "    \"currency\""
    "  ],"
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"brand\":\"brand\","
    "  \"billing_address\":"
    "  {"
    "    \"name\":\"name\","
    "    \"address1\":\"address1\","
    "    \"address2\":\"address2\","
    "    \"city\":\"city\","
    "    \"state\":\"state\","
    "    \"postal_code\":\"postal_code\","
    "    \"phone_number\":\"phone_number\","
    "    \"country_code\":\"country_code\""
    "  },"
    "  \"status\":\"VALID\","
    "  \"object_id\":\"object_id\""
    "}";

const char kMaskedInstrumentMissingAddress[] =
    "{"
    "  \"descriptive_name\":\"descriptive_name\","
    "  \"type\":\"VISA\","
    "  \"supported_currency\":"
    "  ["
    "    \"currency\""
    "  ],"
    "  \"last_four_digits\":\"last_four_digits\","
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"brand\":\"brand\","
    "  \"status\":\"VALID\","
    "  \"object_id\":\"object_id\""
    "}";

const char kMaskedInstrumentMalformedAddress[] =
    "{"
    "  \"descriptive_name\":\"descriptive_name\","
    "  \"type\":\"VISA\","
    "  \"supported_currency\":"
    "  ["
    "    \"currency\""
    "  ],"
    "  \"last_four_digits\":\"last_four_digits\","
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"brand\":\"brand\","
    "  \"billing_address\":"
    "  {"
    "    \"address1\":\"address1\","
    "    \"address2\":\"address2\","
    "    \"city\":\"city\","
    "    \"state\":\"state\","
    "    \"phone_number\":\"phone_number\","
    "    \"country_code\":\"country_code\""
    "  },"
    "  \"status\":\"VALID\","
    "  \"object_id\":\"object_id\""
    "}";

const char kMaskedInstrumentMissingObjectId[] =
    "{"
    "  \"descriptive_name\":\"descriptive_name\","
    "  \"type\":\"VISA\","
    "  \"supported_currency\":"
    "  ["
    "    \"currency\""
    "  ],"
    "  \"last_four_digits\":\"last_four_digits\","
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"brand\":\"brand\","
    "  \"billing_address\":"
    "  {"
    "    \"name\":\"name\","
    "    \"address1\":\"address1\","
    "    \"address2\":\"address2\","
    "    \"city\":\"city\","
    "    \"state\":\"state\","
    "    \"postal_code\":\"postal_code\","
    "    \"phone_number\":\"phone_number\","
    "    \"country_code\":\"country_code\""
    "  },"
    "  \"status\":\"VALID\""
    "}";

const char kLegalDocument[] =
    "{"
    "  \"legal_document_id\":\"doc_id\","
    "  \"display_name\":\"display_name\","
    "  \"document\":\"doc_body\""
    "}";

const char kLegalDocumentMissingDocumentId[] =
    "{"
    "  \"display_name\":\"display_name\","
    "  \"document\":\"doc_body\""
    "}";

const char kLegalDocumentMissingDisplayName[] =
    "{"
    "  \"legal_document_id\":\"doc_id\","
    "  \"document\":\"doc_body\""
    "}";

const char kLegalDocumentMissingDocumentBody[] =
    "{"
    "  \"legal_document_id\":\"doc_id\","
    "  \"display_name\":\"display_name\""
    "}";

const char kWalletItemsWithRequiredActions[] =
    "{"
    "  \"google_transaction_id\":\"google_transaction_id\","
    "  \"required_action\":"
    "  ["
    "    \"  setup_wallet\","
    "    \"AcCePt_ToS  \","
    "    \"  \\tGAIA_auth   \\n\\r\","
    "    \"INVALID_form_field\""
    "  ]"
    "}";

const char kWalletItemsWithInvalidRequiredActions[] =
    "{"
    "  \"google_transaction_id\":\"google_transaction_id\","
    "  \"required_action\":"
    "  ["
    "    \"cvc_risk_CHALLENGE\","
    "    \"UPGRADE_MIN_ADDRESS\","
    "    \"update_EXPIRATION_date\","
    "    \" 忍者の正体 \""
    "  ]"
    "}";

const char kWalletItems[] =
    "{"
    "  \"required_action\":"
    "  ["
    "  ],"
    "  \"google_transaction_id\":\"google_transaction_id\","
    "  \"instrument\":"
    "  ["
    "    {"
    "      \"descriptive_name\":\"descriptive_name\","
    "      \"type\":\"VISA\","
    "      \"supported_currency\":"
    "      ["
    "        \"currency\""
    "      ],"
    "      \"last_four_digits\":\"last_four_digits\","
    "      \"expiration_month\":12,"
    "      \"expiration_year\":2012,"
    "      \"brand\":\"brand\","
    "      \"billing_address\":"
    "      {"
    "        \"name\":\"name\","
    "        \"address1\":\"address1\","
    "        \"address2\":\"address2\","
    "        \"city\":\"city\","
    "        \"state\":\"state\","
    "        \"postal_code\":\"postal_code\","
    "        \"phone_number\":\"phone_number\","
    "        \"country_code\":\"country_code\""
    "      },"
    "      \"status\":\"VALID\","
    "      \"object_id\":\"object_id\""
    "    }"
    "  ],"
    "  \"default_instrument_id\":\"default_instrument_id\","
    "  \"address\":"
    "  ["
    "    {"
    "      \"id\":\"id\","
    "      \"phone_number\":\"phone_number\","
    "      \"postal_address\":"
    "      {"
    "        \"recipient_name\":\"recipient_name\","
    "        \"address_line_1\":\"address_line_1\","
    "        \"address_line_2\":\"address_line_2\","
    "        \"locality_name\":\"locality_name\","
    "        \"administrative_area_name\":\"administrative_area_name\","
    "        \"postal_code_number\":\"postal_code_number\","
    "        \"country_name_code\":\"country_name_code\""
    "      }"
    "    }"
    "  ],"
    "  \"default_address_id\":\"default_address_id\","
    "  \"required_legal_document\":"
    "  ["
    "    {"
    "      \"legal_document_id\":\"doc_id\","
    "      \"display_name\":\"display_name\","
    "      \"document\":\"doc_body\""
    "    }"
    "  ]"
    "}";

}  // anonymous namespace

namespace wallet {

class WalletItemsTest : public testing::Test {
 public:
  WalletItemsTest() {}
 protected:
  void SetUpDictionary(const std::string& json) {
    scoped_ptr<Value> value(base::JSONReader::Read(json));
    DCHECK(value.get());
    DCHECK(value->IsType(Value::TYPE_DICTIONARY));
    dict.reset(static_cast<DictionaryValue*>(value.release()));
  }
  scoped_ptr<DictionaryValue> dict;
};

TEST_F(WalletItemsTest, CreateMaskedInstrumentMissingStatus) {
  SetUpDictionary(kMaskedInstrumentMissingStatus);
  ASSERT_EQ(NULL,
            WalletItems::MaskedInstrument::CreateMaskedInstrument(*dict).get());
}

TEST_F(WalletItemsTest, CreateMaskedInstrumentMissingType) {
  SetUpDictionary(kMaskedInstrumentMissingType);
  ASSERT_EQ(NULL,
            WalletItems::MaskedInstrument::CreateMaskedInstrument(*dict).get());
}

TEST_F(WalletItemsTest, CreateMaskedInstrumentMissingLastFourDigits) {
  SetUpDictionary(kMaskedInstrumentMissingLastFourDigits);
  ASSERT_EQ(NULL,
            WalletItems::MaskedInstrument::CreateMaskedInstrument(*dict).get());
}

TEST_F(WalletItemsTest, CreateMaskedInstrumentMissingAddress) {
  SetUpDictionary(kMaskedInstrumentMissingAddress);
  ASSERT_EQ(NULL,
            WalletItems::MaskedInstrument::CreateMaskedInstrument(*dict).get());
}

TEST_F(WalletItemsTest, CreateMaskedInstrumentMalformedAddress) {
  SetUpDictionary(kMaskedInstrumentMalformedAddress);
  ASSERT_EQ(NULL,
            WalletItems::MaskedInstrument::CreateMaskedInstrument(*dict).get());
}

TEST_F(WalletItemsTest, CreateMaskedInstrumentMissingObjectId) {
  SetUpDictionary(kMaskedInstrumentMissingObjectId);
  ASSERT_EQ(NULL,
            WalletItems::MaskedInstrument::CreateMaskedInstrument(*dict).get());
}

TEST_F(WalletItemsTest, CreateMaskedInstrument) {
  SetUpDictionary(kMaskedInstrument);
  scoped_ptr<Address> address(new Address("country_code",
                                          "name",
                                          "address1",
                                          "address2",
                                          "city",
                                          "state",
                                          "postal_code",
                                          "phone_number",
                                          ""));
  std::vector<std::string> supported_currencies;
  supported_currencies.push_back("currency");
  WalletItems::MaskedInstrument masked_instrument(
      "descriptive_name",
      WalletItems::MaskedInstrument::VISA,
      supported_currencies,
      "last_four_digits",
      12,
      2012,
      "brand",
      address.Pass(),
      WalletItems::MaskedInstrument::VALID,
      "object_id");
  ASSERT_EQ(masked_instrument,
            *WalletItems::MaskedInstrument::CreateMaskedInstrument(*dict));
}

TEST_F(WalletItemsTest, CreateLegalDocumentMissingDocId) {
  SetUpDictionary(kLegalDocumentMissingDocumentId);
  ASSERT_EQ(NULL, WalletItems::LegalDocument::CreateLegalDocument(*dict).get());
}

TEST_F(WalletItemsTest, CreateLegalDocumentMissingDisplayName) {
  SetUpDictionary(kLegalDocumentMissingDisplayName);
  ASSERT_EQ(NULL, WalletItems::LegalDocument::CreateLegalDocument(*dict).get());
}

TEST_F(WalletItemsTest, CreateLegalDocumentMissingDocBody) {
  SetUpDictionary(kLegalDocumentMissingDocumentBody);
  ASSERT_EQ(NULL, WalletItems::LegalDocument::CreateLegalDocument(*dict).get());
}

TEST_F(WalletItemsTest, CreateLegalDocument) {
  SetUpDictionary(kLegalDocument);
  WalletItems::LegalDocument expected("doc_id", "display_name", "doc_body");
  ASSERT_EQ(expected,
            *WalletItems::LegalDocument::CreateLegalDocument(*dict));
}

TEST_F(WalletItemsTest, CreateWalletItemsWithRequiredActions) {
  SetUpDictionary(kWalletItemsWithRequiredActions);

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(SETUP_WALLET);
  required_actions.push_back(ACCEPT_TOS);
  required_actions.push_back(GAIA_AUTH);
  required_actions.push_back(INVALID_FORM_FIELD);

  WalletItems expected(required_actions, "google_transaction_id", "", "");
  ASSERT_EQ(expected, *WalletItems::CreateWalletItems(*dict));

  DCHECK(!required_actions.empty());
  required_actions.pop_back();
  WalletItems different_required_actions(
      required_actions, "google_transaction_id", "", "");
  ASSERT_NE(expected, different_required_actions);
}

TEST_F(WalletItemsTest, CreateWalletItemsWithInvalidRequiredActions) {
  SetUpDictionary(kWalletItemsWithInvalidRequiredActions);
  ASSERT_EQ(NULL, WalletItems::CreateWalletItems(*dict).get());
}

TEST_F(WalletItemsTest, CreateWalletItems) {
  SetUpDictionary(kWalletItems);
  std::vector<RequiredAction> required_actions;
  WalletItems expected(required_actions,
                       "google_transaction_id",
                       "default_instrument_id",
                       "default_address_id");

  scoped_ptr<Address> billing_address(new Address("country_code",
                                                  "name",
                                                  "address1",
                                                  "address2",
                                                  "city",
                                                  "state",
                                                  "postal_code",
                                                  "phone_number",
                                                  ""));
  std::vector<std::string> supported_currencies;
  supported_currencies.push_back("currency");
  scoped_ptr<WalletItems::MaskedInstrument> masked_instrument(
      new WalletItems::MaskedInstrument("descriptive_name",
                                        WalletItems::MaskedInstrument::VISA,
                                        supported_currencies,
                                        "last_four_digits",
                                        12,
                                        2012,
                                        "brand",
                                        billing_address.Pass(),
                                        WalletItems::MaskedInstrument::VALID,
                                        "object_id"));
  expected.AddInstrument(masked_instrument.Pass());

  scoped_ptr<Address> shipping_address(new Address("country_code",
                                                   "name",
                                                   "address1",
                                                   "address2",
                                                   "city",
                                                   "state",
                                                   "postal_code",
                                                   "phone_number",
                                                   "id"));
  expected.AddAddress(shipping_address.Pass());

  scoped_ptr<WalletItems::LegalDocument> legal_document(
      new WalletItems::LegalDocument("doc_id",
                                     "display_name",
                                     "doc_body"));
  expected.AddLegalDocument(legal_document.Pass());

  ASSERT_EQ(expected, *WalletItems::CreateWalletItems(*dict));
}

}  // namespace wallet

