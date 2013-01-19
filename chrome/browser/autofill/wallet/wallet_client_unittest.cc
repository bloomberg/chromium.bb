// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/wallet/cart.h"
#include "chrome/browser/autofill/wallet/full_wallet.h"
#include "chrome/browser/autofill/wallet/instrument.h"
#include "chrome/browser/autofill/wallet/wallet_client.h"
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

const char kSaveAddressValidRequest[] =
    "{"
        "\"api_key\":\"abcdefg\","
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
        "\"api_key\":\"abcdefg\","
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
        "\"risk_params\":\"\""
      "}";

const char kSaveInstrumentAndAddressValidRequest[] =
    "{"
        "\"api_key\":\"abcdefg\","
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
    "gid=obfuscated_gaia_id&cardNumber=4444444444444448&cvv=123";

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

  void VerifyAndFinishRequest(const net::TestURLFetcherFactory& fetcher_factory,
                              net::HttpStatusCode response_code,
                              const std::string& request_body,
                              const std::string& response_body) {
    net::TestURLFetcher* fetcher = fetcher_factory.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    EXPECT_EQ(request_body, fetcher->upload_data());
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

class MockWalletClientObserver
    : public wallet::WalletClient::WalletClientObserver {
 public:
  MockWalletClientObserver() {}
  ~MockWalletClientObserver() {}

  MOCK_METHOD0(OnDidAcceptLegalDocuments, void());
  MOCK_METHOD2(OnDidEncryptOtp, void(const std::string& encrypted_otp,
                                  const std::string& session_material));
  MOCK_METHOD1(OnDidEscrowSensitiveInformation,
               void(const std::string& escrow_handle));
  MOCK_METHOD1(OnDidGetFullWallet, void(FullWallet* full_wallet));
  MOCK_METHOD1(OnDidGetWalletItems, void(WalletItems* wallet_items));
  MOCK_METHOD1(OnDidSaveAddress, void(const std::string& address_id));
  MOCK_METHOD1(OnDidSaveInstrument, void(const std::string& instrument_id));
  MOCK_METHOD2(OnDidSaveInstrumentAndAddress,
               void(const std::string& instrument_id,
                    const std::string& shipping_address_id));
  MOCK_METHOD0(OnDidSendAutocheckoutStatus, void());
  MOCK_METHOD0(OnWalletError, void());
  MOCK_METHOD0(OnMalformedResponse, void());
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

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.EscrowSensitiveInformation(*instrument,
                                           "obfuscated_gaia_id",
                                           &observer);
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kEscrowSensitiveInformationRequest,
                         "abc");
}

TEST_F(WalletClientTest, EscrowSensitiveInformationFailure) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;
  scoped_ptr<Instrument> instrument = GetTestInstrument();
  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.EscrowSensitiveInformation(*instrument,
                                           "obfuscated_gaia_id",
                                           &observer);
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kEscrowSensitiveInformationRequest,
                         std::string());
}

TEST_F(WalletClientTest, GetFullWallet) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnDidGetFullWallet(testing::NotNull())).Times(1);

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
  EXPECT_CALL(observer, OnDidAcceptLegalDocuments()).Times(1);

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
  EXPECT_CALL(observer, OnDidGetWalletItems(testing::NotNull())).Times(1);

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

TEST_F(WalletClientTest, SaveAddressSucceeded) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnDidSaveAddress("shipping_address_id")).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestShippingAddress();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SaveAddress(*address, &observer);
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
  wallet_client.SaveAddress(*address, &observer);
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
  wallet_client.SaveInstrument(*instrument, "escrow_handle", &observer);
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveInstrumentValidRequest,
                         kSaveInstrumentValidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentFailedMalformedResponse) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SaveInstrument(*instrument, "escrow_handle", &observer);
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
                                         "escrow_handle",
                                         *address,
                                         &observer);
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveInstrumentAndAddressValidRequest,
                         kSaveInstrumentAndAddressValidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressFailedAddressMissing) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  scoped_ptr<Address> address = GetTestShippingAddress();

  WalletClient wallet_client(profile_.GetRequestContext());
  wallet_client.SaveInstrumentAndAddress(*instrument,
                                         "escrow_handle",
                                         *address,
                                         &observer);
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
                                         "escrow_handle",
                                         *address,
                                         &observer);
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveInstrumentAndAddressValidRequest,
                         kSaveInstrumentAndAddressMissingInstrumentResponse);
}

TEST_F(WalletClientTest, SendAutocheckoutOfStatusSuccess) {
  MockWalletClientObserver observer;
  EXPECT_CALL(observer, OnDidSendAutocheckoutStatus()).Times(1);

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
  EXPECT_CALL(observer, OnDidSendAutocheckoutStatus()).Times(1);

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

