// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/content/browser/wallet/gaia_account.h"
#include "components/autofill/content/browser/wallet/required_action.h"
#include "components/autofill/content/browser/wallet/wallet_items.h"
#include "components/autofill/content/browser/wallet/wallet_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

namespace {

const char kMaskedInstrument[] =
    "{"
    "  \"descriptive_name\":\"descriptive_name\","
    "  \"type\":\"VISA\","
    "  \"last_four_digits\":\"last_four_digits\","
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"billing_address\":"
    "  {"
    "    \"name\":\"name\","
    "    \"address1\":\"address1\","
    "    \"address2\":\"address2\","
    "    \"city\":\"city\","
    "    \"dependent_locality_name\":\"burough\","
    "    \"state\":\"state\","
    "    \"postal_code\":\"postal_code\","
    "    \"sorting_code\":\"sorting_code\","
    "    \"phone_number\":\"phone_number\","
    "    \"country_code\":\"US\","
    "    \"type\":\"FULL\","
    "    \"language_code\":\"language_code\""
    "  },"
    "  \"status\":\"VALID\","
    "  \"object_id\":\"object_id\""
    "}";

const char kMaskedInstrumentMissingStatus[] =
    "{"
    "  \"descriptive_name\":\"descriptive_name\","
    "  \"type\":\"VISA\","
    "  \"last_four_digits\":\"last_four_digits\","
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"billing_address\":"
    "  {"
    "    \"name\":\"name\","
    "    \"address1\":\"address1\","
    "    \"address2\":\"address2\","
    "    \"city\":\"city\","
    "    \"dependent_locality_name\":\"burough\","
    "    \"state\":\"state\","
    "    \"postal_code\":\"postal_code\","
    "    \"sorting_code\":\"sorting_code\","
    "    \"phone_number\":\"phone_number\","
    "    \"country_code\":\"US\","
    "    \"language_code\":\"language_code\""
    "  },"
    "  \"object_id\":\"object_id\","
    "  \"amex_disallowed\":true"
    "}";

const char kMaskedInstrumentMissingType[] =
    "{"
    "  \"descriptive_name\":\"descriptive_name\","
    "  \"last_four_digits\":\"last_four_digits\","
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"billing_address\":"
    "  {"
    "    \"name\":\"name\","
    "    \"address1\":\"address1\","
    "    \"address2\":\"address2\","
    "    \"city\":\"city\","
    "    \"dependent_locality_name\":\"burough\","
    "    \"state\":\"state\","
    "    \"postal_code\":\"postal_code\","
    "    \"sorting_code\":\"sorting_code\","
    "    \"phone_number\":\"phone_number\","
    "    \"country_code\":\"US\","
    "    \"language_code\":\"language_code\""
    "  },"
    "  \"status\":\"VALID\","
    "  \"object_id\":\"object_id\""
    "}";

const char kMaskedInstrumentMissingLastFourDigits[] =
    "{"
    "  \"descriptive_name\":\"descriptive_name\","
    "  \"type\":\"VISA\","
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"billing_address\":"
    "  {"
    "    \"name\":\"name\","
    "    \"address1\":\"address1\","
    "    \"address2\":\"address2\","
    "    \"city\":\"city\","
    "    \"dependent_locality_name\":\"burough\","
    "    \"state\":\"state\","
    "    \"postal_code\":\"postal_code\","
    "    \"sorting_code\":\"sorting_code\","
    "    \"phone_number\":\"phone_number\","
    "    \"country_code\":\"US\","
    "    \"language_code\":\"language_code\""
    "  },"
    "  \"status\":\"VALID\","
    "  \"object_id\":\"object_id\""
    "}";

const char kMaskedInstrumentMissingAddress[] =
    "{"
    "  \"descriptive_name\":\"descriptive_name\","
    "  \"type\":\"VISA\","
    "  \"last_four_digits\":\"last_four_digits\","
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"status\":\"VALID\","
    "  \"object_id\":\"object_id\""
    "}";

const char kMaskedInstrumentMalformedAddress[] =
    "{"
    "  \"descriptive_name\":\"descriptive_name\","
    "  \"type\":\"VISA\","
    "  \"last_four_digits\":\"last_four_digits\","
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"billing_address\":"
    "  {"
    "    \"address1\":\"address1\","
    "    \"address2\":\"address2\","
    "    \"city\":\"city\","
    "    \"dependent_locality_name\":\"burough\","
    "    \"state\":\"state\","
    "    \"phone_number\":\"phone_number\","
    "    \"country_code\":\"US\""
    "  },"
    "  \"status\":\"VALID\","
    "  \"object_id\":\"object_id\""
    "}";

const char kMaskedInstrumentMissingObjectId[] =
    "{"
    "  \"descriptive_name\":\"descriptive_name\","
    "  \"type\":\"VISA\","
    "  \"last_four_digits\":\"last_four_digits\","
    "  \"expiration_month\":12,"
    "  \"expiration_year\":2012,"
    "  \"billing_address\":"
    "  {"
    "    \"name\":\"name\","
    "    \"address1\":\"address1\","
    "    \"address2\":\"address2\","
    "    \"city\":\"city\","
    "    \"dependent_locality_name\":\"burough\","
    "    \"state\":\"state\","
    "    \"postal_code\":\"postal_code\","
    "    \"phone_number\":\"phone_number\","
    "    \"country_code\":\"US\","
    "    \"language_code\":\"language_code\""
    "  },"
    "  \"status\":\"VALID\""
    "}";

const char kLegalDocument[] =
    "{"
    "  \"legal_document_id\":\"doc_id\","
    "  \"display_name\":\"display_name\""
    "}";

const char kLegalDocumentMissingDocumentId[] =
    "{"
    "  \"display_name\":\"display_name\""
    "}";

const char kLegalDocumentMissingDisplayName[] =
    "{"
    "  \"legal_document_id\":\"doc_id\""
    "}";

const char kWalletItemsWithRequiredActions[] =
    "{"
    "  \"required_action\":"
    "  ["
    "    \"  setup_wallet\","
    "    \" CHOOse_ANother_INSTRUMENT_OR_ADDRESS\","
    "    \"AcCePt_ToS  \","
    "    \"  \\tGAIA_auth   \\n\\r\","
    "    \"UPDATE_expiration_date\","
    "    \"UPGRADE_min_ADDRESS   \","
    "    \" pAsSiVe_GAIA_auth \","
    "    \" REQUIRE_PHONE_NUMBER\\t \""
    "  ]"
    "}";

const char kWalletItemsWithInvalidRequiredActions[] =
    "{"
    "  \"required_action\":"
    "  ["
    "    \"verify_CVV\","
    "    \"invalid_FORM_FIELD\","
    "    \" 忍者の正体 \""
    "  ]"
    "}";

const char kWalletItemsMissingGoogleTransactionId[] =
    "{"
    "  \"required_action\":"
    "  ["
    "  ],"
    "  \"instrument\":"
    "  ["
    "    {"
    "      \"descriptive_name\":\"descriptive_name\","
    "      \"type\":\"VISA\","
    "      \"last_four_digits\":\"last_four_digits\","
    "      \"expiration_month\":12,"
    "      \"expiration_year\":2012,"
    "      \"billing_address\":"
    "      {"
    "        \"name\":\"name\","
    "        \"address1\":\"address1\","
    "        \"address2\":\"address2\","
    "        \"city\":\"city\","
    "        \"state\":\"state\","
    "        \"postal_code\":\"postal_code\","
    "        \"phone_number\":\"phone_number\","
    "        \"country_code\":\"US\","
    "        \"language_code\":\"language_code\""
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
    "        \"address_line\":"
    "        ["
    "          \"address_line_1\","
    "          \"address_line_2\""
    "        ],"
    "        \"locality_name\":\"locality_name\","
    "        \"administrative_area_name\":\"administrative_area_name\","
    "        \"postal_code_number\":\"postal_code_number\","
    "        \"country_name_code\":\"US\","
    "        \"language_code\":\"language_code\""
    "      }"
    "    }"
    "  ],"
    "  \"default_address_id\":\"default_address_id\","
    "  \"amex_disallowed\":true,"
    "  \"required_legal_document\":"
    "  ["
    "    {"
    "      \"legal_document_id\":\"doc_id\","
    "      \"display_name\":\"display_name\""
    "    }"
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
    "      \"last_four_digits\":\"last_four_digits\","
    "      \"expiration_month\":12,"
    "      \"expiration_year\":2012,"
    "      \"billing_address\":"
    "      {"
    "        \"name\":\"name\","
    "        \"address1\":\"address1\","
    "        \"address2\":\"address2\","
    "        \"city\":\"city\","
    "        \"dependent_locality_name\":\"burough\","
    "        \"state\":\"state\","
    "        \"postal_code\":\"postal_code\","
    "        \"sorting_code\":\"sorting_code\","
    "        \"phone_number\":\"phone_number\","
    "        \"country_code\":\"US\","
    "        \"type\":\"FULL\","
    "        \"language_code\":\"language_code\""
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
    "        \"address_line\":"
    "        ["
    "          \"address_line_1\","
    "          \"address_line_2\""
    "        ],"
    "        \"locality_name\":\"locality_name\","
    "        \"dependent_locality_name\":\"dependent_locality_name\","
    "        \"administrative_area_name\":\"administrative_area_name\","
    "        \"postal_code_number\":\"postal_code_number\","
    "        \"sorting_code\":\"sorting_code\","
    "        \"country_name_code\":\"US\","
    "        \"language_code\":\"language_code\""
    "      }"
    "    }"
    "  ],"
    "  \"default_address_id\":\"default_address_id\","
    "  \"obfuscated_gaia_id\":\"ignore_this_value\","
    "  \"amex_disallowed\":true,"
    "  \"gaia_profile\":"
    "  ["
    "    {"
    "      \"buyer_email\":\"user@chromium.org\","
    "      \"gaia_index\":0,"
    "      \"gaia_id\":\"123456789\","
    "      \"buyer_name\":\"Joe Usecase\","
    "      \"is_active\":true,"
    "      \"avatar_url_27x27\":\"https://lh3.googleusercontent.com/27.jpg\","
    "      \"avatar_url_54x54\":\"https://lh3.googleusercontent.com/54.jpg\","
    "      \"avatar_url_48x48\":\"https://lh3.googleusercontent.com/48.jpg\","
    "      \"avatar_url_96x96\":\"https://lh3.googleusercontent.com/96.jpg\""
    "    },"
    "    {"
    "      \"buyer_email\":\"user2@chromium.org\","
    "      \"gaia_index\":1,"
    "      \"gaia_id\":\"obfuscated_gaia_id\","
    "      \"buyer_name\":\"Jill Usecase\","
    "      \"is_active\":false,"
    "      \"avatar_url_27x27\":\"https://lh3.googleusercontent.com/27.jpg\","
    "      \"avatar_url_54x54\":\"https://lh3.googleusercontent.com/54.jpg\","
    "      \"avatar_url_48x48\":\"https://lh3.googleusercontent.com/48.jpg\","
    "      \"avatar_url_96x96\":\"https://lh3.googleusercontent.com/96.jpg\""
    "    }"
    "  ],"
    "  \"allowed_shipping_spec_by_country\":"
    "  ["
    "    {\"country_code\":\"AC\"},"
    "    {\"country_code\":\"AD\"},"
    "    {\"country_code\":\"US\"}"
    "  ]";

const char kRequiredLegalDocument[] =
    "  ,"
    "  \"required_legal_document\":"
    "  ["
    "    {"
    "      \"legal_document_id\":\"doc_id\","
    "      \"display_name\":\"display_name\""
    "    }"
    "  ]";

const char kCloseJson[] = "}";

}  // anonymous namespace

namespace autofill {
namespace wallet {

class WalletItemsTest : public testing::Test {
 public:
  WalletItemsTest() {}
 protected:
  void SetUpDictionary(const std::string& json) {
    scoped_ptr<base::Value> value(base::JSONReader::Read(json));
    ASSERT_TRUE(value.get());
    ASSERT_TRUE(value->IsType(base::Value::TYPE_DICTIONARY));
    dict.reset(static_cast<base::DictionaryValue*>(value.release()));
  }
  scoped_ptr<base::DictionaryValue> dict;
};

TEST_F(WalletItemsTest, CreateMaskedInstrumentMissingStatus) {
  SetUpDictionary(kMaskedInstrumentMissingStatus);
  EXPECT_EQ(NULL,
            WalletItems::MaskedInstrument::CreateMaskedInstrument(*dict).get());
}

TEST_F(WalletItemsTest, CreateMaskedInstrumentMissingType) {
  SetUpDictionary(kMaskedInstrumentMissingType);
  EXPECT_EQ(NULL,
            WalletItems::MaskedInstrument::CreateMaskedInstrument(*dict).get());
}

TEST_F(WalletItemsTest, CreateMaskedInstrumentMissingLastFourDigits) {
  SetUpDictionary(kMaskedInstrumentMissingLastFourDigits);
  EXPECT_EQ(NULL,
            WalletItems::MaskedInstrument::CreateMaskedInstrument(*dict).get());
}

TEST_F(WalletItemsTest, CreateMaskedInstrumentMissingAddress) {
  SetUpDictionary(kMaskedInstrumentMissingAddress);
  EXPECT_EQ(NULL,
            WalletItems::MaskedInstrument::CreateMaskedInstrument(*dict).get());
}

TEST_F(WalletItemsTest, CreateMaskedInstrumentMalformedAddress) {
  SetUpDictionary(kMaskedInstrumentMalformedAddress);
  EXPECT_EQ(NULL,
            WalletItems::MaskedInstrument::CreateMaskedInstrument(*dict).get());
}

TEST_F(WalletItemsTest, CreateMaskedInstrumentMissingObjectId) {
  SetUpDictionary(kMaskedInstrumentMissingObjectId);
  EXPECT_EQ(NULL,
            WalletItems::MaskedInstrument::CreateMaskedInstrument(*dict).get());
}

TEST_F(WalletItemsTest, CreateMaskedInstrument) {
  SetUpDictionary(kMaskedInstrument);
  scoped_ptr<Address> address(
      new Address("US",
                  ASCIIToUTF16("name"),
                  StreetAddress("address1", "address2"),
                  ASCIIToUTF16("city"),
                  ASCIIToUTF16("burough"),
                  ASCIIToUTF16("state"),
                  ASCIIToUTF16("postal_code"),
                  ASCIIToUTF16("sorting_code"),
                  ASCIIToUTF16("phone_number"),
                  std::string(),
                  "language_code"));
  WalletItems::MaskedInstrument masked_instrument(
      ASCIIToUTF16("descriptive_name"),
      WalletItems::MaskedInstrument::VISA,
      ASCIIToUTF16("last_four_digits"),
      12,
      2012,
      address.Pass(),
      WalletItems::MaskedInstrument::VALID,
      "object_id");
  EXPECT_EQ(masked_instrument,
            *WalletItems::MaskedInstrument::CreateMaskedInstrument(*dict));
}

TEST_F(WalletItemsTest, CreateLegalDocumentMissingDocId) {
  SetUpDictionary(kLegalDocumentMissingDocumentId);
  EXPECT_EQ(NULL, WalletItems::LegalDocument::CreateLegalDocument(*dict).get());
}

TEST_F(WalletItemsTest, CreateLegalDocumentMissingDisplayName) {
  SetUpDictionary(kLegalDocumentMissingDisplayName);
  EXPECT_EQ(NULL, WalletItems::LegalDocument::CreateLegalDocument(*dict).get());
}

TEST_F(WalletItemsTest, CreateLegalDocument) {
  SetUpDictionary(kLegalDocument);
  WalletItems::LegalDocument expected("doc_id", ASCIIToUTF16("display_name"));
  EXPECT_EQ(expected,
            *WalletItems::LegalDocument::CreateLegalDocument(*dict));
}

TEST_F(WalletItemsTest, LegalDocumentUrl) {
  WalletItems::LegalDocument legal_doc("doc_id", ASCIIToUTF16("display_name"));
  EXPECT_EQ("https://wallet.google.com/legaldocument?docId=doc_id",
            legal_doc.url().spec());
}

TEST_F(WalletItemsTest, LegalDocumentEmptyId) {
  WalletItems::LegalDocument legal_doc(GURL("http://example.com"),
                                       ASCIIToUTF16("display_name"));
  EXPECT_TRUE(legal_doc.id().empty());
}

TEST_F(WalletItemsTest, CreateWalletItemsWithRequiredActions) {
  SetUpDictionary(kWalletItemsWithRequiredActions);

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(SETUP_WALLET);
  required_actions.push_back(CHOOSE_ANOTHER_INSTRUMENT_OR_ADDRESS);
  required_actions.push_back(ACCEPT_TOS);
  required_actions.push_back(GAIA_AUTH);
  required_actions.push_back(UPDATE_EXPIRATION_DATE);
  required_actions.push_back(UPGRADE_MIN_ADDRESS);
  required_actions.push_back(PASSIVE_GAIA_AUTH);
  required_actions.push_back(REQUIRE_PHONE_NUMBER);

  WalletItems expected(required_actions,
                       std::string(),
                       std::string(),
                       std::string(),
                       AMEX_DISALLOWED);
  EXPECT_EQ(expected, *WalletItems::CreateWalletItems(*dict));

  ASSERT_FALSE(required_actions.empty());
  required_actions.pop_back();
  WalletItems different_required_actions(required_actions,
                                         std::string(),
                                         std::string(),
                                         std::string(),
                                         AMEX_DISALLOWED);
  EXPECT_NE(expected, different_required_actions);
}

TEST_F(WalletItemsTest, CreateWalletItemsWithInvalidRequiredActions) {
  SetUpDictionary(kWalletItemsWithInvalidRequiredActions);
  EXPECT_EQ(NULL, WalletItems::CreateWalletItems(*dict).get());
}

TEST_F(WalletItemsTest, CreateWalletItemsMissingGoogleTransactionId) {
  SetUpDictionary(kWalletItemsMissingGoogleTransactionId);
  EXPECT_EQ(NULL, WalletItems::CreateWalletItems(*dict).get());
}

TEST_F(WalletItemsTest, CreateWalletItemsMissingAmexDisallowed) {
  SetUpDictionary(std::string(kWalletItems) + std::string(kCloseJson));
  EXPECT_TRUE(dict->Remove("amex_disallowed", NULL));
  base::string16 amex_number = ASCIIToUTF16("378282246310005");
  base::string16 message;
  EXPECT_FALSE(WalletItems::CreateWalletItems(*dict)->SupportsCard(amex_number,
                                                                   &message));
  EXPECT_FALSE(message.empty());
}

TEST_F(WalletItemsTest, CreateWalletItems) {
  SetUpDictionary(std::string(kWalletItems) + std::string(kCloseJson));
  std::vector<RequiredAction> required_actions;
  WalletItems expected(required_actions,
                       "google_transaction_id",
                       "default_instrument_id",
                       "default_address_id",
                       AMEX_DISALLOWED);

  scoped_ptr<GaiaAccount> user1(GaiaAccount::CreateForTesting(
      "user@chromium.org",
      "123456789",
      0,
      true));
  expected.AddAccount(user1.Pass());
  scoped_ptr<GaiaAccount> user2(GaiaAccount::CreateForTesting(
      "user2@chromium.org",
      "obfuscated_gaia_id",
      1,
      false));
  expected.AddAccount(user2.Pass());
  EXPECT_EQ("123456789", expected.ObfuscatedGaiaId());

  scoped_ptr<Address> billing_address(
      new Address("US",
                  ASCIIToUTF16("name"),
                  StreetAddress("address1", "address2"),
                  ASCIIToUTF16("city"),
                  ASCIIToUTF16("burough"),
                  ASCIIToUTF16("state"),
                  ASCIIToUTF16("postal_code"),
                  ASCIIToUTF16("sorting_code"),
                  ASCIIToUTF16("phone_number"),
                  std::string(),
                  "language_code"));
  scoped_ptr<WalletItems::MaskedInstrument> masked_instrument(
      new WalletItems::MaskedInstrument(ASCIIToUTF16("descriptive_name"),
                                        WalletItems::MaskedInstrument::VISA,
                                        ASCIIToUTF16("last_four_digits"),
                                        12,
                                        2012,
                                        billing_address.Pass(),
                                        WalletItems::MaskedInstrument::VALID,
                                        "object_id"));
  expected.AddInstrument(masked_instrument.Pass());

  scoped_ptr<Address> shipping_address(
      new Address("US",
                  ASCIIToUTF16("recipient_name"),
                  StreetAddress("address_line_1", "address_line_2"),
                  ASCIIToUTF16("locality_name"),
                  ASCIIToUTF16("dependent_locality_name"),
                  ASCIIToUTF16("administrative_area_name"),
                  ASCIIToUTF16("postal_code_number"),
                  ASCIIToUTF16("sorting_code"),
                  ASCIIToUTF16("phone_number"),
                  "id",
                  "language_code"));
  expected.AddAddress(shipping_address.Pass());

  expected.AddAllowedShippingCountry("AC");
  expected.AddAllowedShippingCountry("AD");
  expected.AddAllowedShippingCountry("US");

  EXPECT_EQ(expected, *WalletItems::CreateWalletItems(*dict));

  // Now try with a legal document as well.
  SetUpDictionary(std::string(kWalletItems) +
                  std::string(kRequiredLegalDocument) +
                  std::string(kCloseJson));
  scoped_ptr<WalletItems::LegalDocument> legal_document(
      new WalletItems::LegalDocument("doc_id",
                                     ASCIIToUTF16("display_name")));
  expected.AddLegalDocument(legal_document.Pass());
  expected.AddLegalDocument(
      WalletItems::LegalDocument::CreatePrivacyPolicyDocument());

  EXPECT_EQ(expected, *WalletItems::CreateWalletItems(*dict));
}

}  // namespace wallet
}  // namespace autofill
