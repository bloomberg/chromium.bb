// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/wallet/cart.h"
#include "chrome/browser/autofill/wallet/full_wallet.h"
#include "chrome/browser/autofill/wallet/instrument.h"
#include "chrome/browser/autofill/wallet/wallet_client.h"
#include "chrome/browser/autofill/wallet/wallet_client_observer.h"
#include "chrome/browser/autofill/wallet/wallet_items.h"
#include "chrome/browser/autofill/wallet/wallet_test_util.h"
#include "chrome/common/autofill/autocheckout_status.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kGoogleTransactionId[] = "google-transaction-id";
const char kMerchantUrl[] = "https://example.com/path?key=value";

const char kGetFullWalletValidResponse[] =
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
    "      \"administrative_area_name\":\"ship_administrative_area_name\","
    "      \"postal_code_number\":\"ship_postal_code_number\","
    "      \"country_name_code\":\"ship_country_name_code\""
    "    }"
    "  },"
    "  \"required_action\":"
    "  ["
    "  ]"
    "}";

const char kGetFullWalletInvalidResponse[] =
    "{"
    "  \"garbage\":123"
    "}";

const char kGetWalletItemsValidResponse[] =
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
    "      \"supported_currency\":\"currency_code\","
    "      \"last_four_digits\":\"last_four_digits\","
    "      \"expiration_month\":12,"
    "      \"expiration_year\":2012,"
    "      \"brand\":\"monkeys\","
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
    "      \"object_id\":\"default_instrument_id\""
    "    }"
    "  ],"
    "  \"default_instrument_id\":\"default_instrument_id\","
    "  \"obfuscated_gaia_id\":\"obfuscated_gaia_id\","
    "  \"address\":"
    "  ["
    "  ],"
    "  \"default_address_id\":\"default_address_id\","
    "  \"required_legal_document\":"
    "  ["
    "  ]"
    "}";

const char kSaveAddressValidResponse[] =
    "{"
    "  \"shipping_address_id\":\"shipping_address_id\""
    "}";

const char kSaveInvalidResponse[] =
    "{"
    "  \"garbage\":123"
    "}";

const char kSaveInstrumentValidResponse[] =
    "{"
    "  \"instrument_id\":\"instrument_id\""
    "}";

const char kSaveInstrumentAndAddressValidResponse[] =
    "{"
    "  \"shipping_address_id\":\"shipping_address_id\","
    "  \"instrument_id\":\"instrument_id\""
    "}";

const char kSaveInstrumentAndAddressMissingAddressResponse[] =
    "{"
    "  \"instrument_id\":\"instrument_id\""
    "}";

const char kSaveInstrumentAndAddressMissingInstrumentResponse[] =
    "{"
    "  \"shipping_address_id\":\"shipping_address_id\""
    "}";

const char kUpdateInstrumentValidResponse[] =
    "{"
    "  \"instrument_id\":\"instrument_id\""
    "}";

const char kUpdateInstrumentMalformedResponse[] =
    "{"
    "  \"cheese\":\"monkeys\""
    "}";

const char kAuthenticateInstrumentFailureResponse[] =
    "{"
    "  \"auth_result\":\"anything else\""
    "}";

const char kAuthenticateInstrumentSuccessResponse[] =
    "{"
    "  \"auth_result\":\"SUCCESS\""
    "}";

// The JSON below is used to test against the request payload being sent to
// Online Wallet. It's indented differently since JSONWriter creates compact
// JSON from DictionaryValues.

const char kAcceptLegalDocumentsValidRequest[] =
    "{"
        "\"accepted_legal_document\":"
        "["
            "\"doc_1\","
            "\"doc_2\""
        "],"
        "\"google_transaction_id\":\"google-transaction-id\","
        "\"merchant_domain\":\"https://example.com/\""
    "}";

const char kAuthenticateInstrumentValidRequest[] =
    "{"
        "\"instrument_escrow_handle\":\"escrow_handle\","
        "\"instrument_id\":\"instrument_id\","
        "\"risk_params\":\"\""
    "}";

const char kGetFullWalletValidRequest[] =
    "{"
        "\"cart\":"
        "{"
            "\"currency_code\":\"currency_code\","
            "\"total_price\":\"currency_code\""
        "},"
        "\"encrypted_otp\":\"encrypted_one_time_pad\","
        "\"google_transaction_id\":\"google_transaction_id\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"risk_params\":\"\","
        "\"selected_address_id\":\"shipping_address_id\","
        "\"selected_instrument_id\":\"instrument_id\","
        "\"session_material\":\"session_material\""
    "}";

const char kGetWalletItemsValidRequest[] =
    "{"
        "\"merchant_domain\":\"https://example.com/\","
        "\"risk_params\":\"\""
    "}";

const char kSaveAddressValidRequest[] =
    "{"
        "\"merchant_domain\":\"https://example.com/\","
        "\"risk_params\":\"\","
        "\"shipping_address\":"
        "{"
            "\"phone_number\":\"ship_phone_number\","
            "\"postal_address\":"
            "{"
                "\"address_line\":"
                "["
                    "\"ship_address_line_1\","
                    "\"ship_address_line_2\""
                "],"
                "\"administrative_area_name\":\"ship_admin_area_name\","
                "\"country_name_code\":\"ship_country_name_code\","
                "\"locality_name\":\"ship_locality_name\","
                "\"postal_code_number\":\"ship_postal_code_number\","
                "\"recipient_name\":\"ship_recipient_name\""
            "}"
        "}"
    "}";

const char kSaveInstrumentValidRequest[] =
    "{"
        "\"instrument\":"
        "{"
            "\"credit_card\":"
            "{"
                "\"address\":"
                "{"
                    "\"address_line\":"
                    "["
                        "\"address_line_1\","
                        "\"address_line_2\""
                    "],"
                    "\"administrative_area_name\":\"admin_area_name\","
                    "\"country_name_code\":\"country_name_code\","
                    "\"locality_name\":\"locality_name\","
                    "\"postal_code_number\":\"postal_code_number\","
                    "\"recipient_name\":\"recipient_name\""
                "},"
                "\"exp_month\":12,"
                "\"exp_year\":2012,"
                "\"fop_type\":\"VISA\","
                "\"last_4_digits\":\"4448\""
            "},"
            "\"type\":\"CREDIT_CARD\""
        "},"
        "\"instrument_escrow_handle\":\"escrow_handle\","
        "\"instrument_phone_number\":\"phone_number\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"risk_params\":\"\""
      "}";

const char kSaveInstrumentAndAddressValidRequest[] =
    "{"
        "\"instrument\":"
        "{"
            "\"credit_card\":"
            "{"
                "\"address\":"
                "{"
                    "\"address_line\":"
                    "["
                        "\"address_line_1\","
                        "\"address_line_2\""
                    "],"
                    "\"administrative_area_name\":\"admin_area_name\","
                    "\"country_name_code\":\"country_name_code\","
                    "\"locality_name\":\"locality_name\","
                    "\"postal_code_number\":\"postal_code_number\","
                    "\"recipient_name\":\"recipient_name\""
                "},"
                "\"exp_month\":12,"
                "\"exp_year\":2012,"
                "\"fop_type\":\"VISA\","
                "\"last_4_digits\":\"4448\""
            "},"
            "\"type\":\"CREDIT_CARD\""
        "},"
        "\"instrument_escrow_handle\":\"escrow_handle\","
        "\"instrument_phone_number\":\"phone_number\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"risk_params\":\"\","
        "\"shipping_address\":"
        "{"
            "\"phone_number\":\"ship_phone_number\","
            "\"postal_address\":"
            "{"
                "\"address_line\":"
                "["
                    "\"ship_address_line_1\","
                    "\"ship_address_line_2\""
                "],"
                "\"administrative_area_name\":\"ship_admin_area_name\","
                "\"country_name_code\":\"ship_country_name_code\","
                "\"locality_name\":\"ship_locality_name\","
                "\"postal_code_number\":\"ship_postal_code_number\","
                "\"recipient_name\":\"ship_recipient_name\""
            "}"
        "}"
    "}";

const char kSendAutocheckoutStatusOfSuccessValidRequest[] =
    "{"
        "\"google_transaction_id\":\"google_transaction_id\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"success\":true"
    "}";

const char kSendAutocheckoutStatusOfFailureValidRequest[] =
    "{"
        "\"google_transaction_id\":\"google_transaction_id\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"reason\":\"CANNOT_PROCEED\","
        "\"success\":false"
    "}";

const char kUpdateInstrumentValidRequest[] =
    "{"
        "\"instrument_phone_number\":\"phone_number\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"risk_params\":\"\","
        "\"upgraded_billing_address\":"
        "{"
            "\"address_line\":"
            "["
                "\"address_line_1\","
                "\"address_line_2\""
            "],"
            "\"administrative_area_name\":\"admin_area_name\","
            "\"country_name_code\":\"country_name_code\","
            "\"locality_name\":\"locality_name\","
            "\"postal_code_number\":\"postal_code_number\","
            "\"recipient_name\":\"recipient_name\""
        "},"
        "\"upgraded_instrument_id\":\"instrument_id\""
    "}";

}  // namespace

namespace autofill {
namespace wallet {

class WalletClientTest : public testing::Test {
 public:
  WalletClientTest() : io_thread_(content::BrowserThread::IO) {}

  virtual void SetUp() {
    io_thread_.StartIOThread();
    profile_.CreateRequestContext();
  }

  virtual void TearDown() {
    profile_.ResetRequestContext();
    io_thread_.Stop();
  }

  std::string GetData(net::TestURLFetcher* fetcher) {
    std::string data = fetcher->upload_data();
    scoped_ptr<Value> root(base::JSONReader::Read(data));

    // If this is not a JSON dictionary, return plain text.
    if (root.get() == NULL || !root->IsType(Value::TYPE_DICTIONARY))
      return data;

    // Remove api_key entry (to prevent accidental leak), return JSON as text.
    DictionaryValue* dict = static_cast<DictionaryValue*>(root.get());
    dict->Remove("api_key", NULL);
    base::JSONWriter::Write(dict, &data);
    return data;
  }

  void VerifyAndFinishRequest(const net::TestURLFetcherFactory& fetcher_factory,
                              net::HttpStatusCode response_code,
                              const std::string& request_body,
                              const std::string& response_body) {
    net::TestURLFetcher* fetcher = fetcher_factory.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    EXPECT_EQ(request_body, GetData(fetcher));
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response_body);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }
 protected:
  TestingProfile profile_;

 private:
  // The profile's request context must be released on the IO thread.
  content::TestBrowserThread io_thread_;
};

class MockWalletClientObserver :
  public WalletClientObserver,
  public base::SupportsWeakPtr<MockWalletClientObserver> {
 public:
  MockWalletClientObserver()
      : full_wallets_received_(0), wallet_items_received_(0) {}
  ~MockWalletClientObserver() {}

  MOCK_METHOD0(OnDidAcceptLegalDocuments, void());
  MOCK_METHOD1(OnDidAuthenticateInstrument, void(bool success));
  MOCK_METHOD1(OnDidSaveAddress, void(const std::string& address_id));
  MOCK_METHOD1(OnDidSaveInstrument, void(const std::string& instrument_id));
  MOCK_METHOD2(OnDidSaveInstrumentAndAddress,
               void(const std::string& instrument_id,
                    const std::string& shipping_address_id));
  MOCK_METHOD0(OnDidSendAutocheckoutStatus, void());
  MOCK_METHOD1(OnDidUpdateInstrument, void(const std::string& instrument_id));
  MOCK_METHOD0(OnWalletError, void());
  MOCK_METHOD0(OnMalformedResponse, void());
  MOCK_METHOD1(OnNetworkError, void(int response_code));

  virtual void OnDidGetFullWallet(scoped_ptr<FullWallet> full_wallet) OVERRIDE {
    EXPECT_TRUE(full_wallet);
    ++full_wallets_received_;
  }
  virtual void OnDidGetWalletItems(scoped_ptr<WalletItems> wallet_items)
      OVERRIDE {
    EXPECT_TRUE(wallet_items);
    ++wallet_items_received_;
  }
  size_t full_wallets_received() const { return full_wallets_received_; }
  size_t wallet_items_received() const { return wallet_items_received_; }

 private:
  size_t full_wallets_received_;
  size_t wallet_items_received_;
};

// TODO(ahutter): Implement API compatibility tests. See
// http://crbug.com/164465.

// TODO(ahutter): Improve this when the error body is captured. See
// http://crbug.com/164410.
TEST_F(WalletClientTest, WalletErrorOnExpectedVoidResponse) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnWalletError()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SendAutocheckoutStatus(autofill::SUCCESS,
                                       GURL(kMerchantUrl),
                                       "",
                                       observer.AsWeakPtr());
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_INTERNAL_SERVER_ERROR);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

// TODO(ahutter): Improve this when the error body is captured. See
// http://crbug.com/164410.
TEST_F(WalletClientTest, WalletErrorOnExpectedResponse) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnWalletError()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.GetWalletItems(GURL(kMerchantUrl), observer.AsWeakPtr());
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_INTERNAL_SERVER_ERROR);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, NetworkFailureOnExpectedVoidResponse) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnNetworkError(net::HTTP_UNAUTHORIZED)).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SendAutocheckoutStatus(autofill::SUCCESS,
                                       GURL(kMerchantUrl),
                                       "",
                                       observer.AsWeakPtr());
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, NetworkFailureOnExpectedResponse) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnNetworkError(net::HTTP_UNAUTHORIZED)).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.GetWalletItems(GURL(kMerchantUrl), observer.AsWeakPtr());
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, RequestError) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnWalletError()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SendAutocheckoutStatus(autofill::SUCCESS,
                                       GURL(kMerchantUrl),
                                       "",
                                       observer.AsWeakPtr());
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_BAD_REQUEST);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, GetFullWalletSuccess) {
  MockWalletClientObserver observer;
  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  Cart cart("currency_code", "currency_code");
  wallet_client.GetFullWallet("instrument_id",
                              "shipping_address_id",
                              GURL(kMerchantUrl),
                              cart,
                              "google_transaction_id",
                              observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString(
      "session_material|encrypted_one_time_pad");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kGetFullWalletValidRequest,
                         kGetFullWalletValidResponse);
  EXPECT_EQ(1U, observer.full_wallets_received());
}

TEST_F(WalletClientTest, GetFullWalletEncryptionDown) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer,
              OnNetworkError(net::HTTP_INTERNAL_SERVER_ERROR)).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  Cart cart("currency_code", "currency_code");
  wallet_client.GetFullWallet("instrument_id",
                              "shipping_address_id",
                              GURL(kMerchantUrl),
                              cart,
                              "google_transaction_id",
                              observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_INTERNAL_SERVER_ERROR);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  EXPECT_EQ(0U, observer.full_wallets_received());
}

TEST_F(WalletClientTest, GetFullWalletEncryptionMalformed) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  Cart cart("currency_code", "currency_code");
  wallet_client.GetFullWallet("instrument_id",
                              "shipping_address_id",
                              GURL(kMerchantUrl),
                              cart,
                              "google_transaction_id",
                              observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString(
      "session_material:encrypted_one_time_pad");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  EXPECT_EQ(0U, observer.full_wallets_received());
}

TEST_F(WalletClientTest, GetFullWalletMalformedResponse) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  Cart cart("currency_code", "currency_code");
  wallet_client.GetFullWallet("instrument_id",
                              "shipping_address_id",
                              GURL(kMerchantUrl),
                              cart,
                              "google_transaction_id",
                              observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString(
      "session_material|encrypted_one_time_pad");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kGetFullWalletValidRequest,
                         kGetFullWalletInvalidResponse);
  EXPECT_EQ(0U, observer.full_wallets_received());
}

TEST_F(WalletClientTest, AcceptLegalDocuments) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnDidAcceptLegalDocuments()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  std::vector<std::string> doc_ids;
  doc_ids.push_back("doc_1");
  doc_ids.push_back("doc_2");
  wallet_client.AcceptLegalDocuments(doc_ids,
                                     kGoogleTransactionId,
                                     GURL(kMerchantUrl),
                                     observer.AsWeakPtr());
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(kAcceptLegalDocumentsValidRequest, GetData(fetcher));
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, AuthenticateInstrumentSucceeded) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnDidAuthenticateInstrument(true)).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.AuthenticateInstrument("instrument_id",
                                       "cvv",
                                       "obfuscated_gaia_id",
                                       observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kAuthenticateInstrumentValidRequest,
                         kAuthenticateInstrumentSuccessResponse);
}

TEST_F(WalletClientTest, AuthenticateInstrumentFailed) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnDidAuthenticateInstrument(false)).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.AuthenticateInstrument("instrument_id",
                                       "cvv",
                                       "obfuscated_gaia_id",
                                       observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kAuthenticateInstrumentValidRequest,
                         kAuthenticateInstrumentFailureResponse);
}

TEST_F(WalletClientTest, AuthenticateInstrumentEscrowDown) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer,
              OnNetworkError(net::HTTP_INTERNAL_SERVER_ERROR)).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.AuthenticateInstrument("instrument_id",
                                       "cvv",
                                       "obfuscated_gaia_id",
                                       observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_INTERNAL_SERVER_ERROR);
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, AuthenticateInstrumentEscrowMalformed) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.AuthenticateInstrument("instrument_id",
                                       "cvv",
                                       "obfuscated_gaia_id",
                                       observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, AuthenticateInstrumentFailedMalformedResponse) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.AuthenticateInstrument("instrument_id",
                                       "cvv",
                                       "obfuscated_gaia_id",
                                       observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kAuthenticateInstrumentValidRequest,
                         kSaveInvalidResponse);
}

// TODO(ahutter): Add failure tests for GetWalletItems.

TEST_F(WalletClientTest, GetWalletItems) {
  MockWalletClientObserver observer;
  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.GetWalletItems(GURL(kMerchantUrl), observer.AsWeakPtr());
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(kGetWalletItemsValidRequest, GetData(fetcher));
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(kGetWalletItemsValidResponse);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  EXPECT_EQ(1U, observer.wallet_items_received());
}

TEST_F(WalletClientTest, SaveAddressSucceeded) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnDidSaveAddress("shipping_address_id")).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestShippingAddress();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SaveAddress(*address, GURL(kMerchantUrl), observer.AsWeakPtr());
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveAddressValidRequest,
                         kSaveAddressValidResponse);
}

TEST_F(WalletClientTest, SaveAddressFailedMalformedResponse) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestShippingAddress();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SaveAddress(*address, GURL(kMerchantUrl), observer.AsWeakPtr());
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveAddressValidRequest,
                         kSaveInvalidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentSucceeded) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnDidSaveInstrument("instrument_id")).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SaveInstrument(*instrument,
                               "obfuscated_gaia_id",
                               GURL(kMerchantUrl),
                               observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveInstrumentValidRequest,
                         kSaveInstrumentValidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentEscrowDown) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer,
              OnNetworkError(net::HTTP_INTERNAL_SERVER_ERROR)).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SaveInstrument(*instrument,
                               "obfuscated_gaia_id",
                               GURL(kMerchantUrl),
                               observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_INTERNAL_SERVER_ERROR);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, SaveInstrumentEscrowMalformed) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SaveInstrument(*instrument,
                               "obfuscated_gaia_id",
                               GURL(kMerchantUrl),
                               observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, SaveInstrumentFailedMalformedResponse) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SaveInstrument(*instrument,
                               "obfuscated_gaia_id",
                               GURL(kMerchantUrl),
                               observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveInstrumentValidRequest,
                         kSaveInvalidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressSucceeded) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer,
              OnDidSaveInstrumentAndAddress("instrument_id",
                                            "shipping_address_id")).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  scoped_ptr<Address> address = GetTestShippingAddress();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SaveInstrumentAndAddress(*instrument,
                                         *address,
                                         "obfuscated_gaia_id",
                                         GURL(kMerchantUrl),
                                         observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveInstrumentAndAddressValidRequest,
                         kSaveInstrumentAndAddressValidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressEscrowDown) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer,
              OnNetworkError(net::HTTP_INTERNAL_SERVER_ERROR)).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  scoped_ptr<Address> address = GetTestShippingAddress();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SaveInstrumentAndAddress(*instrument,
                                         *address,
                                         "obfuscated_gaia_id",
                                         GURL(kMerchantUrl),
                                         observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_INTERNAL_SERVER_ERROR);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressEscrowMalformed) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  scoped_ptr<Address> address = GetTestShippingAddress();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SaveInstrumentAndAddress(*instrument,
                                         *address,
                                         "obfuscated_gaia_id",
                                         GURL(kMerchantUrl),
                                         observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressFailedAddressMissing) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  scoped_ptr<Address> address = GetTestShippingAddress();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SaveInstrumentAndAddress(*instrument,
                                         *address,
                                         "obfuscated_gaia_id",
                                         GURL(kMerchantUrl),
                                         observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveInstrumentAndAddressValidRequest,
                         kSaveInstrumentAndAddressMissingAddressResponse);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressFailedInstrumentMissing) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  scoped_ptr<Address> address = GetTestShippingAddress();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SaveInstrumentAndAddress(*instrument,
                                         *address,
                                         "obfuscated_gaia_id",
                                         GURL(kMerchantUrl),
                                         observer.AsWeakPtr());

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveInstrumentAndAddressValidRequest,
                         kSaveInstrumentAndAddressMissingInstrumentResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentSucceeded) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnDidUpdateInstrument("instrument_id")).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestAddress();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.UpdateInstrument("instrument_id",
                                 *address,
                                 GURL(kMerchantUrl),
                                 observer.AsWeakPtr());
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kUpdateInstrumentValidRequest,
                         kUpdateInstrumentValidResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentMalformedResponse) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestAddress();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.UpdateInstrument("instrument_id",
                                 *address,
                                 GURL(kMerchantUrl),
                                 observer.AsWeakPtr());
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kUpdateInstrumentValidRequest,
                         kUpdateInstrumentMalformedResponse);
}

TEST_F(WalletClientTest, SendAutocheckoutOfStatusSuccess) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnDidSendAutocheckoutStatus()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SendAutocheckoutStatus(autofill::SUCCESS,
                                       GURL(kMerchantUrl),
                                       "google_transaction_id",
                                       observer.AsWeakPtr());
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(kSendAutocheckoutStatusOfSuccessValidRequest, GetData(fetcher));
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, SendAutocheckoutStatusOfFailure) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnDidSendAutocheckoutStatus()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SendAutocheckoutStatus(autofill::CANNOT_PROCEED,
                                       GURL(kMerchantUrl),
                                       "google_transaction_id",
                                       observer.AsWeakPtr());
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(kSendAutocheckoutStatusOfFailureValidRequest, GetData(fetcher));
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, HasRequestInProgress) {
  MockWalletClientObserver observer;
  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  EXPECT_FALSE(wallet_client.HasRequestInProgress());

  wallet_client.GetWalletItems(GURL(kMerchantUrl), observer.AsWeakPtr());
  EXPECT_TRUE(wallet_client.HasRequestInProgress());

  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(kGetWalletItemsValidResponse);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_FALSE(wallet_client.HasRequestInProgress());
}

}  // namespace wallet
}  // namespace autofill
