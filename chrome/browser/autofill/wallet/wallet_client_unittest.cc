// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/wallet/cart.h"
#include "chrome/browser/autofill/wallet/full_wallet.h"
#include "chrome/browser/autofill/wallet/wallet_client.h"
#include "chrome/browser/autofill/wallet/wallet_items.h"
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
        "\"api_key\":\"abcdefg\","
        "\"google_transaction_id\":\"google-transaction-id\""
    "}";

const char kGetFullWalletValidRequest[] =
    "{"
        "\"api_key\":\"abcdefg\","
        "\"cart\":"
        "{"
            "\"currency_code\":\"currency_code\","
            "\"total_price\":\"currency_code\""
        "},"
        "\"encrypted_otp\":\"encrypted_otp\","
        "\"google_transaction_id\":\"google_transaction_id\","
        "\"merchant_domain\":\"merchant_domain\","
        "\"risk_params\":\"\","
        "\"selected_address_id\":\"shipping_address_id\","
        "\"selected_instrument_id\":\"instrument_id\","
        "\"session_material\":\"session_material\""
    "}";

const char kGetWalletItemsValidRequest[] =
    "{"
        "\"api_key\":\"abcdefg\","
        "\"risk_params\":\"\""
    "}";

const char kSendAutocheckoutStatusOfSuccessValidRequest[] =
    "{"
        "\"api_key\":\"abcdefg\","
        "\"google_transaction_id\":\"google_transaction_id\","
        "\"hostname\":\"hostname\","
        "\"success\":true"
    "}";

const char kSendAutocheckoutStatusOfFailureValidRequest[] =
    "{"
        "\"api_key\":\"abcdefg\","
        "\"google_transaction_id\":\"google_transaction_id\","
        "\"hostname\":\"hostname\","
        "\"reason\":\"CANNOT_PROCEED\","
        "\"success\":false"
    "}";

const char kEscrowSensitiveInformationRequest[] =
    "gid=obfuscated_gaia_id&cardNumber=pan&cvv=cvn";

}  // anonymous namespace

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

 protected:
  TestingProfile profile_;

 private:
  // The profile's request context must be released on the IO thread.
  content::TestBrowserThread io_thread_;
};

class MockWalletClientObserver
    : public wallet::WalletClient::WalletClientObserver {
 public:
  MockWalletClientObserver() {}
  ~MockWalletClientObserver() {}

  MOCK_METHOD0(OnAcceptLegalDocuments, void());
  MOCK_METHOD2(OnEncryptOtp, void(const std::string& encrypted_otp,
                                  const std::string& session_material));
  MOCK_METHOD1(OnDidEscrowSensitiveInformation,
               void(const std::string& escrow_handle));
  MOCK_METHOD1(OnGetFullWallet, void(FullWallet* full_wallet));
  MOCK_METHOD1(OnGetWalletItems, void(WalletItems* wallet_items));
  MOCK_METHOD0(OnSendAutocheckoutStatus, void());
  MOCK_METHOD0(OnWalletError, void());
  MOCK_METHOD1(OnNetworkError, void(int response_code));
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
                                       "",
                                       "",
                                       &observer);
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
  Cart cart("currency_code", "currency_code");
  wallet_client.GetFullWallet("", "", "", cart, "", "", "", &observer);
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
                                       "",
                                       "",
                                       &observer);
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
  Cart cart("currency_code", "currency_code");
  wallet_client.GetFullWallet("", "", "", cart, "", "", "", &observer);
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
                                       "",
                                       "",
                                       &observer);
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_BAD_REQUEST);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

// TODO(ahutter): Add test for EncryptOtp.
// TODO(ahutter): Add failure tests for EncryptOtp, GetWalletItems,
// GetFullWallet for when data is missing or invalid.

TEST_F(WalletClientTest, EscrowSensitiveInformationSuccess) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnDidEscrowSensitiveInformation("abc")).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.EscrowSensitiveInformation("pan",
                                           "cvn",
                                           "obfuscated_gaia_id",
                                           &observer);
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(kEscrowSensitiveInformationRequest, fetcher->upload_data());
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString("abc");
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, EscrowSensitiveInformationFailure) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnWalletError()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.EscrowSensitiveInformation("pan",
                                           "cvn",
                                           "obfuscated_gaia_id",
                                           &observer);
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(kEscrowSensitiveInformationRequest, fetcher->upload_data());
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString("");
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, GetFullWallet) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnGetFullWallet(testing::NotNull())).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  Cart cart("currency_code", "currency_code");
  wallet_client.GetFullWallet("instrument_id",
                              "shipping_address_id",
                              "merchant_domain",
                              cart,
                              "google_transaction_id",
                              "encrypted_otp",
                              "session_material",
                              &observer);
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(kGetFullWalletValidRequest, fetcher->upload_data());
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(kGetFullWalletValidResponse);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, AcceptLegalDocuments) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnAcceptLegalDocuments()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  std::vector<std::string> doc_ids;
  doc_ids.push_back("doc_1");
  doc_ids.push_back("doc_2");
  wallet_client.AcceptLegalDocuments(doc_ids,
                                     kGoogleTransactionId,
                                     &observer);
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(kAcceptLegalDocumentsValidRequest, fetcher->upload_data());
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, GetWalletItems) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnGetWalletItems(testing::NotNull())).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.GetWalletItems(&observer);
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(kGetWalletItemsValidRequest, fetcher->upload_data());
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(kGetWalletItemsValidResponse);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, SendAutocheckoutOfStatusSuccess) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnSendAutocheckoutStatus()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SendAutocheckoutStatus(autofill::SUCCESS,
                                       "hostname",
                                       "google_transaction_id",
                                       &observer);
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(kSendAutocheckoutStatusOfSuccessValidRequest,
            fetcher->upload_data());
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, SendAutocheckoutStatusOfFailure) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnSendAutocheckoutStatus()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SendAutocheckoutStatus(autofill::CANNOT_PROCEED,
                                       "hostname",
                                       "google_transaction_id",
                                       &observer);
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(kSendAutocheckoutStatusOfFailureValidRequest,
            fetcher->upload_data());
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

}  // namespace wallet

