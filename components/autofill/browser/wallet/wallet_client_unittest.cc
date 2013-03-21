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
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/browser/autofill_metrics.h"
#include "components/autofill/browser/wallet/cart.h"
#include "components/autofill/browser/wallet/full_wallet.h"
#include "components/autofill/browser/wallet/instrument.h"
#include "components/autofill/browser/wallet/wallet_client.h"
#include "components/autofill/browser/wallet/wallet_client_delegate.h"
#include "components/autofill/browser/wallet/wallet_items.h"
#include "components/autofill/browser/wallet/wallet_test_util.h"
#include "components/autofill/common/autocheckout_status.h"
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
    "  \"shipping_address_id\":\"saved_address_id\""
    "}";

const char kSaveAddressWithRequiredActionsValidResponse[] =
    "{"
    "  \"required_action\":"
    "  ["
    "    \"  \\treqUIRE_PhOnE_number   \\n\\r\","
    "    \"INVALID_form_field\""
    "  ]"
    "}";

const char kSaveWithInvalidRequiredActionsResponse[] =
    "{"
    "  \"required_action\":"
    "  ["
    "    \"  setup_wallet\","
    "    \"  \\treqUIRE_PhOnE_number   \\n\\r\","
    "    \"INVALID_form_field\""
    "  ]"
    "}";

const char kSaveInvalidResponse[] =
    "{"
    "  \"garbage\":123"
    "}";

const char kSaveInstrumentValidResponse[] =
    "{"
    "  \"instrument_id\":\"instrument_id\""
    "}";

const char kSaveInstrumentWithRequiredActionsValidResponse[] =
    "{"
    "  \"required_action\":"
    "  ["
    "    \"  \\treqUIRE_PhOnE_number   \\n\\r\","
    "    \"INVALID_form_field\""
    "  ]"
    "}";

const char kSaveInstrumentAndAddressValidResponse[] =
    "{"
    "  \"shipping_address_id\":\"saved_address_id\","
    "  \"instrument_id\":\"saved_instrument_id\""
    "}";

const char kSaveInstrumentAndAddressWithRequiredActionsValidResponse[] =
    "{"
    "  \"required_action\":"
    "  ["
    "    \"  \\treqUIRE_PhOnE_number   \\n\\r\","
    "    \"INVALID_form_field\""
    "  ]"
    "}";

const char kSaveInstrumentAndAddressMissingAddressResponse[] =
    "{"
    "  \"instrument_id\":\"instrument_id\""
    "}";

const char kSaveInstrumentAndAddressMissingInstrumentResponse[] =
    "{"
    "  \"shipping_address_id\":\"saved_address_id\""
    "}";

const char kUpdateInstrumentValidResponse[] =
    "{"
    "  \"instrument_id\":\"instrument_id\""
    "}";

const char kUpdateAddressValidResponse[] =
    "{"
    "  \"shipping_address_id\":\"shipping_address_id\""
    "}";

const char kUpdateWithRequiredActionsValidResponse[] =
    "{"
    "  \"required_action\":"
    "  ["
    "    \"  \\treqUIRE_PhOnE_number   \\n\\r\","
    "    \"INVALID_form_field\""
    "  ]"
    "}";

const char kUpdateMalformedResponse[] =
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

const char kErrorResponse[] =
    "{"
    "  \"error_type\":\"APPLICATION_ERROR\","
    "  \"error_detail\":\"error_detail\","
    "  \"application_error\":\"application_error\","
    "  \"debug_data\":"
    "  {"
    "    \"debug_message\":\"debug_message\","
    "    \"stack_trace\":\"stack_trace\""
    "  },"
    "  \"application_error_data\":\"application_error_data\","
    "  \"wallet_error\":"
    "  {"
    "    \"error_type\":\"SERVICE_UNAVAILABLE\","
    "    \"error_detail\":\"error_detail\","
    "    \"message_for_user\":"
    "    {"
    "      \"text\":\"text\","
    "      \"subtext\":\"subtext\","
    "      \"details\":\"details\""
    "    }"
    "  }"
    "}";

const char kErrorTypeMissingInResponse[] =
    "{"
    "  \"error_type\":\"Not APPLICATION_ERROR\","
    "  \"error_detail\":\"error_detail\","
    "  \"application_error\":\"application_error\","
    "  \"debug_data\":"
    "  {"
    "    \"debug_message\":\"debug_message\","
    "    \"stack_trace\":\"stack_trace\""
    "  },"
    "  \"application_error_data\":\"application_error_data\""
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
        "\"risk_params\":\"risky business\""
    "}";

const char kGetFullWalletValidRequest[] =
    "{"
        "\"cart\":"
        "{"
            "\"currency_code\":\"currency_code\","
            "\"total_price\":\"total_price\""
        "},"
        "\"encrypted_otp\":\"encrypted_one_time_pad\","
        "\"feature\":\"REQUEST_AUTOCOMPLETE\","
        "\"google_transaction_id\":\"google_transaction_id\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"risk_params\":\"risky business\","
        "\"selected_address_id\":\"shipping_address_id\","
        "\"selected_instrument_id\":\"instrument_id\","
        "\"session_material\":\"session_material\","
        "\"supported_risk_challenge\":"
        "["
        "]"
    "}";

const char kGetFullWalletWithRiskCapabilitesValidRequest[] =
    "{"
        "\"cart\":"
        "{"
            "\"currency_code\":\"currency_code\","
            "\"total_price\":\"total_price\""
        "},"
        "\"encrypted_otp\":\"encrypted_one_time_pad\","
        "\"feature\":\"REQUEST_AUTOCOMPLETE\","
        "\"google_transaction_id\":\"google_transaction_id\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"risk_params\":\"risky business\","
        "\"selected_address_id\":\"shipping_address_id\","
        "\"selected_instrument_id\":\"instrument_id\","
        "\"session_material\":\"session_material\","
        "\"supported_risk_challenge\":"
        "["
            "\"VERIFY_CVC\""
        "]"
    "}";

const char kGetWalletItemsValidRequest[] =
    "{"
        "\"merchant_domain\":\"https://example.com/\","
        "\"risk_params\":\"risky business\","
        "\"supported_risk_challenge\":"
        "["
        "]"
    "}";

const char kGetWalletItemsWithRiskCapabilitiesValidRequest[] =
    "{"
        "\"merchant_domain\":\"https://example.com/\","
        "\"risk_params\":\"risky business\","
        "\"supported_risk_challenge\":"
        "["
            "\"RELOGIN\""
        "]"
    "}";

const char kSaveAddressValidRequest[] =
    "{"
        "\"merchant_domain\":\"https://example.com/\","
        "\"risk_params\":\"risky business\","
        "\"shipping_address\":"
        "{"
            "\"phone_number\":\"save_phone_number\","
            "\"postal_address\":"
            "{"
                "\"address_line\":"
                "["
                    "\"save_address_line_1\","
                    "\"save_address_line_2\""
                "],"
                "\"administrative_area_name\":\"save_admin_area_name\","
                "\"country_name_code\":\"save_country_name_code\","
                "\"locality_name\":\"save_locality_name\","
                "\"postal_code_number\":\"save_postal_code_number\","
                "\"recipient_name\":\"save_recipient_name\""
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
        "\"risk_params\":\"risky business\""
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
        "\"risk_params\":\"risky business\","
        "\"shipping_address\":"
        "{"
            "\"phone_number\":\"save_phone_number\","
            "\"postal_address\":"
            "{"
                "\"address_line\":"
                "["
                    "\"save_address_line_1\","
                    "\"save_address_line_2\""
                "],"
                "\"administrative_area_name\":\"save_admin_area_name\","
                "\"country_name_code\":\"save_country_name_code\","
                "\"locality_name\":\"save_locality_name\","
                "\"postal_code_number\":\"save_postal_code_number\","
                "\"recipient_name\":\"save_recipient_name\""
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


const char kUpdateAddressValidRequest[] =
    "{"
        "\"merchant_domain\":\"https://example.com/\","
        "\"risk_params\":\"risky business\","
        "\"shipping_address\":"
        "{"
            "\"id\":\"shipping_address_id\","
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

const char kUpdateInstrumentValidRequest[] =
    "{"
        "\"instrument_phone_number\":\"phone_number\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"risk_params\":\"risky business\","
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

class MockWalletClientDelegate : public WalletClientDelegate {
 public:
  MockWalletClientDelegate()
      : full_wallets_received_(0), wallet_items_received_(0) {}
  ~MockWalletClientDelegate() {}

  virtual const AutofillMetrics& GetMetricLogger() const OVERRIDE {
    return metric_logger_;
  }

  virtual DialogType GetDialogType() const OVERRIDE {
    return DIALOG_TYPE_REQUEST_AUTOCOMPLETE;
  }

  MOCK_METHOD0(OnDidAcceptLegalDocuments, void());
  MOCK_METHOD1(OnDidAuthenticateInstrument, void(bool success));
  MOCK_METHOD2(OnDidSaveAddress,
               void(const std::string& address_id,
                    const std::vector<RequiredAction>& required_actions));
  MOCK_METHOD2(OnDidSaveInstrument,
               void(const std::string& instrument_id,
                    const std::vector<RequiredAction>& required_actions));
  MOCK_METHOD3(OnDidSaveInstrumentAndAddress,
               void(const std::string& instrument_id,
                    const std::string& shipping_address_id,
                    const std::vector<RequiredAction>& required_actions));
  MOCK_METHOD0(OnDidSendAutocheckoutStatus, void());
  MOCK_METHOD2(OnDidUpdateAddress,
               void(const std::string& address_id,
                    const std::vector<RequiredAction>& required_actions));
  MOCK_METHOD2(OnDidUpdateInstrument,
               void(const std::string& instrument_id,
                    const std::vector<RequiredAction>& required_actions));
  MOCK_METHOD1(OnWalletError, void(WalletClient::ErrorType error_type));
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

  AutofillMetrics metric_logger_;
};

// TODO(ahutter): Implement API compatibility tests. See
// http://crbug.com/164465.

TEST_F(WalletClientTest, WalletError) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnWalletError(
      WalletClient::SERVICE_UNAVAILABLE)).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SendAutocheckoutStatus(autofill::SUCCESS,
                                       GURL(kMerchantUrl),
                                       "google_transaction_id");
  VerifyAndFinishRequest(factory,
                         net::HTTP_INTERNAL_SERVER_ERROR,
                         kSendAutocheckoutStatusOfSuccessValidRequest,
                         kErrorResponse);
}

TEST_F(WalletClientTest, WalletErrorResponseMissing) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnWalletError(
      WalletClient::UNKNOWN_ERROR)).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SendAutocheckoutStatus(autofill::SUCCESS,
                                       GURL(kMerchantUrl),
                                       "google_transaction_id");
  VerifyAndFinishRequest(factory,
                         net::HTTP_INTERNAL_SERVER_ERROR,
                         kSendAutocheckoutStatusOfSuccessValidRequest,
                         kErrorTypeMissingInResponse);
}

TEST_F(WalletClientTest, NetworkFailureOnExpectedVoidResponse) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnNetworkError(net::HTTP_UNAUTHORIZED)).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SendAutocheckoutStatus(autofill::SUCCESS,
                                       GURL(kMerchantUrl),
                                       "");
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, NetworkFailureOnExpectedResponse) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnNetworkError(net::HTTP_UNAUTHORIZED)).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.GetWalletItems(GURL(kMerchantUrl),
                               std::vector<WalletClient::RiskCapability>());
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, RequestError) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnWalletError(WalletClient::BAD_REQUEST)).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SendAutocheckoutStatus(autofill::SUCCESS,
                                       GURL(kMerchantUrl),
                                       "");
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_BAD_REQUEST);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, GetFullWalletSuccess) {
  MockWalletClientDelegate delegate;
  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  Cart cart("total_price", "currency_code");
  WalletClient::FullWalletRequest full_wallet_request(
      "instrument_id",
      "shipping_address_id",
      GURL(kMerchantUrl),
      cart,
      "google_transaction_id",
      std::vector<WalletClient::RiskCapability>());
  wallet_client.GetFullWallet(full_wallet_request);

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
  EXPECT_EQ(1U, delegate.full_wallets_received());
}

TEST_F(WalletClientTest, GetFullWalletWithRiskCapabilitesSuccess) {
  MockWalletClientDelegate delegate;
  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  std::vector<WalletClient::RiskCapability> risk_capabilities;
  risk_capabilities.push_back(WalletClient::VERIFY_CVC);
  Cart cart("total_price", "currency_code");
  WalletClient::FullWalletRequest full_wallet_request(
      "instrument_id",
      "shipping_address_id",
      GURL(kMerchantUrl),
      cart,
      "google_transaction_id",
      risk_capabilities);
  wallet_client.GetFullWallet(full_wallet_request);

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString(
      "session_material|encrypted_one_time_pad");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kGetFullWalletWithRiskCapabilitesValidRequest,
                         kGetFullWalletValidResponse);
  EXPECT_EQ(1U, delegate.full_wallets_received());
}

TEST_F(WalletClientTest, GetFullWalletEncryptionDown) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate,
              OnNetworkError(net::HTTP_INTERNAL_SERVER_ERROR)).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  Cart cart("total_price", "currency_code");
  WalletClient::FullWalletRequest full_wallet_request(
      "instrument_id",
      "shipping_address_id",
      GURL(kMerchantUrl),
      cart,
      "google_transaction_id",
      std::vector<WalletClient::RiskCapability>());
  wallet_client.GetFullWallet(full_wallet_request);

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_INTERNAL_SERVER_ERROR);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  EXPECT_EQ(0U, delegate.full_wallets_received());
}

TEST_F(WalletClientTest, GetFullWalletEncryptionMalformed) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  Cart cart("total_price", "currency_code");
  WalletClient::FullWalletRequest full_wallet_request(
      "instrument_id",
      "shipping_address_id",
      GURL(kMerchantUrl),
      cart,
      "google_transaction_id",
      std::vector<WalletClient::RiskCapability>());
  wallet_client.GetFullWallet(full_wallet_request);

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString(
      "session_material:encrypted_one_time_pad");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  EXPECT_EQ(0U, delegate.full_wallets_received());
}

TEST_F(WalletClientTest, GetFullWalletMalformedResponse) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  Cart cart("total_price", "currency_code");
  WalletClient::FullWalletRequest full_wallet_request(
      "instrument_id",
      "shipping_address_id",
      GURL(kMerchantUrl),
      cart,
      "google_transaction_id",
      std::vector<WalletClient::RiskCapability>());
  wallet_client.GetFullWallet(full_wallet_request);

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
  EXPECT_EQ(0U, delegate.full_wallets_received());
}

TEST_F(WalletClientTest, AcceptLegalDocuments) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnDidAcceptLegalDocuments()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  std::vector<std::string> doc_ids;
  doc_ids.push_back("doc_1");
  doc_ids.push_back("doc_2");
  wallet_client.AcceptLegalDocuments(doc_ids,
                                     kGoogleTransactionId,
                                     GURL(kMerchantUrl));
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(kAcceptLegalDocumentsValidRequest, GetData(fetcher));
  fetcher->SetResponseString(")]}'");  // Invalid JSON. Should be ignored.
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, AuthenticateInstrumentSucceeded) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnDidAuthenticateInstrument(true)).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.AuthenticateInstrument("instrument_id",
                                       "cvv",
                                       "obfuscated_gaia_id");

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
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnDidAuthenticateInstrument(false)).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.AuthenticateInstrument("instrument_id",
                                       "cvv",
                                       "obfuscated_gaia_id");

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
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate,
              OnNetworkError(net::HTTP_INTERNAL_SERVER_ERROR)).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.AuthenticateInstrument("instrument_id",
                                       "cvv",
                                       "obfuscated_gaia_id");

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_INTERNAL_SERVER_ERROR);
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, AuthenticateInstrumentEscrowMalformed) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.AuthenticateInstrument("instrument_id",
                                       "cvv",
                                       "obfuscated_gaia_id");

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, AuthenticateInstrumentFailedMalformedResponse) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.AuthenticateInstrument("instrument_id",
                                       "cvv",
                                       "obfuscated_gaia_id");

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
  MockWalletClientDelegate delegate;
  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.GetWalletItems(GURL(kMerchantUrl),
                               std::vector<WalletClient::RiskCapability>());

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kGetWalletItemsValidRequest,
                         kGetWalletItemsValidResponse);
  EXPECT_EQ(1U, delegate.wallet_items_received());
}

TEST_F(WalletClientTest, GetWalletItemsWithRiskCapabilites) {
  MockWalletClientDelegate delegate;
  net::TestURLFetcherFactory factory;

  std::vector<WalletClient::RiskCapability> risk_capabilities;
  risk_capabilities.push_back(WalletClient::RELOGIN);

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.GetWalletItems(GURL(kMerchantUrl),
                               risk_capabilities);

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kGetWalletItemsWithRiskCapabilitiesValidRequest,
                         kGetWalletItemsValidResponse);
  EXPECT_EQ(1U, delegate.wallet_items_received());
}

TEST_F(WalletClientTest, SaveAddressSucceeded) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate,
              OnDidSaveAddress("saved_address_id",
                               std::vector<RequiredAction>())).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestSaveableAddress();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveAddressValidRequest,
                         kSaveAddressValidResponse);
}

TEST_F(WalletClientTest, SaveAddressWithRequiredActionsSucceeded) {
  MockWalletClientDelegate delegate;

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(REQUIRE_PHONE_NUMBER);
  required_actions.push_back(INVALID_FORM_FIELD);

  EXPECT_CALL(delegate,
              OnDidSaveAddress(std::string(),
                               required_actions)).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestSaveableAddress();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveAddressValidRequest,
                         kSaveAddressWithRequiredActionsValidResponse);
}

TEST_F(WalletClientTest, SaveAddressFailedInvalidRequiredAction) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestSaveableAddress();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveAddressValidRequest,
                         kSaveWithInvalidRequiredActionsResponse);
}

TEST_F(WalletClientTest, SaveAddressFailedMalformedResponse) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestSaveableAddress();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveAddressValidRequest,
                         kSaveInvalidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentSucceeded) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate,
              OnDidSaveInstrument("instrument_id",
                                  std::vector<RequiredAction>())).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveInstrument(*instrument,
                               "obfuscated_gaia_id",
                               GURL(kMerchantUrl));

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

TEST_F(WalletClientTest, SaveInstrumentWithRequiredActionsSucceeded) {
  MockWalletClientDelegate delegate;

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(REQUIRE_PHONE_NUMBER);
  required_actions.push_back(INVALID_FORM_FIELD);

  EXPECT_CALL(delegate,
              OnDidSaveInstrument(std::string(),
                                  required_actions)).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveInstrument(*instrument,
                               "obfuscated_gaia_id",
                               GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveInstrumentValidRequest,
                         kSaveInstrumentWithRequiredActionsValidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentFailedInvalidRequiredActions) {
  MockWalletClientDelegate delegate;

  EXPECT_CALL(delegate, OnMalformedResponse());

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveInstrument(*instrument,
                               "obfuscated_gaia_id",
                               GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveInstrumentValidRequest,
                         kSaveWithInvalidRequiredActionsResponse);
}

TEST_F(WalletClientTest, SaveInstrumentEscrowDown) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate,
              OnNetworkError(net::HTTP_INTERNAL_SERVER_ERROR)).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveInstrument(*instrument,
                               "obfuscated_gaia_id",
                               GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_INTERNAL_SERVER_ERROR);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, SaveInstrumentEscrowMalformed) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveInstrument(*instrument,
                               "obfuscated_gaia_id",
                               GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, SaveInstrumentFailedMalformedResponse) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveInstrument(*instrument,
                               "obfuscated_gaia_id",
                               GURL(kMerchantUrl));

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
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate,
              OnDidSaveInstrumentAndAddress(
                  "saved_instrument_id",
                  "saved_address_id",
                  std::vector<RequiredAction>())).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  scoped_ptr<Address> address = GetTestSaveableAddress();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveInstrumentAndAddress(*instrument,
                                         *address,
                                         "obfuscated_gaia_id",
                                         GURL(kMerchantUrl));

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

TEST_F(WalletClientTest, SaveInstrumentAndAddressWithRequiredActionsSucceeded) {
  MockWalletClientDelegate delegate;

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(REQUIRE_PHONE_NUMBER);
  required_actions.push_back(INVALID_FORM_FIELD);

  EXPECT_CALL(delegate,
              OnDidSaveInstrumentAndAddress(
                  std::string(),
                  std::string(),
                  required_actions)).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  scoped_ptr<Address> address = GetTestSaveableAddress();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveInstrumentAndAddress(*instrument,
                                         *address,
                                         "obfuscated_gaia_id",
                                         GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
  VerifyAndFinishRequest(
      factory,
      net::HTTP_OK,
      kSaveInstrumentAndAddressValidRequest,
      kSaveInstrumentAndAddressWithRequiredActionsValidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressFailedInvalidRequiredAction) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  scoped_ptr<Address> address = GetTestSaveableAddress();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveInstrumentAndAddress(*instrument,
                                         *address,
                                         "obfuscated_gaia_id",
                                         GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kSaveInstrumentAndAddressValidRequest,
                         kSaveWithInvalidRequiredActionsResponse);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressEscrowDown) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate,
              OnNetworkError(net::HTTP_INTERNAL_SERVER_ERROR)).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  scoped_ptr<Address> address = GetTestSaveableAddress();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveInstrumentAndAddress(*instrument,
                                         *address,
                                         "obfuscated_gaia_id",
                                         GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_INTERNAL_SERVER_ERROR);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressEscrowMalformed) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  scoped_ptr<Address> address = GetTestSaveableAddress();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveInstrumentAndAddress(*instrument,
                                         *address,
                                         "obfuscated_gaia_id",
                                         GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressFailedAddressMissing) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  scoped_ptr<Address> address = GetTestSaveableAddress();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveInstrumentAndAddress(*instrument,
                                         *address,
                                         "obfuscated_gaia_id",
                                         GURL(kMerchantUrl));

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
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();

  scoped_ptr<Address> address = GetTestSaveableAddress();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SaveInstrumentAndAddress(*instrument,
                                         *address,
                                         "obfuscated_gaia_id",
                                         GURL(kMerchantUrl));

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

TEST_F(WalletClientTest, UpdateAddressSucceeded) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate,
              OnDidUpdateAddress("shipping_address_id",
                                 std::vector<RequiredAction>())).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestShippingAddress();
  address->set_object_id("shipping_address_id");

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.UpdateAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kUpdateAddressValidRequest,
                         kUpdateAddressValidResponse);
}

TEST_F(WalletClientTest, UpdateAddressWithRequiredActionsSucceeded) {
  MockWalletClientDelegate delegate;

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(REQUIRE_PHONE_NUMBER);
  required_actions.push_back(INVALID_FORM_FIELD);

  EXPECT_CALL(delegate,
              OnDidUpdateAddress(std::string(), required_actions)).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestShippingAddress();
  address->set_object_id("shipping_address_id");

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.UpdateAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kUpdateAddressValidRequest,
                         kUpdateWithRequiredActionsValidResponse);
}

TEST_F(WalletClientTest, UpdateAddressFailedInvalidRequiredAction) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestShippingAddress();
  address->set_object_id("shipping_address_id");

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.UpdateAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kUpdateAddressValidRequest,
                         kSaveWithInvalidRequiredActionsResponse);
}

TEST_F(WalletClientTest, UpdateAddressMalformedResponse) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestShippingAddress();
  address->set_object_id("shipping_address_id");

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.UpdateAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kUpdateAddressValidRequest,
                         kUpdateMalformedResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentSucceeded) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate,
              OnDidUpdateInstrument("instrument_id",
                                    std::vector<RequiredAction>())).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestAddress();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.UpdateInstrument("instrument_id",
                                 *address,
                                 GURL(kMerchantUrl));
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kUpdateInstrumentValidRequest,
                         kUpdateInstrumentValidResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentWithRequiredActionsSucceeded) {
  MockWalletClientDelegate delegate;

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(REQUIRE_PHONE_NUMBER);
  required_actions.push_back(INVALID_FORM_FIELD);

  EXPECT_CALL(delegate,
              OnDidUpdateInstrument(std::string(),
                                    required_actions)).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestAddress();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.UpdateInstrument("instrument_id",
                                 *address,
                                 GURL(kMerchantUrl));
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kUpdateInstrumentValidRequest,
                         kUpdateWithRequiredActionsValidResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentFailedInvalidRequiredAction) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestAddress();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.UpdateInstrument("instrument_id",
                                 *address,
                                 GURL(kMerchantUrl));
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kUpdateInstrumentValidRequest,
                         kSaveWithInvalidRequiredActionsResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentMalformedResponse) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Address> address = GetTestAddress();

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.UpdateInstrument("instrument_id",
                                 *address,
                                 GURL(kMerchantUrl));
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kUpdateInstrumentValidRequest,
                         kUpdateMalformedResponse);
}

TEST_F(WalletClientTest, SendAutocheckoutOfStatusSuccess) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnDidSendAutocheckoutStatus()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SendAutocheckoutStatus(autofill::SUCCESS,
                                       GURL(kMerchantUrl),
                                       "google_transaction_id");
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(kSendAutocheckoutStatusOfSuccessValidRequest, GetData(fetcher));
  fetcher->SetResponseString(")]}'");  // Invalid JSON. Should be ignored.
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, SendAutocheckoutStatusOfFailure) {
  MockWalletClientDelegate delegate;
  EXPECT_CALL(delegate, OnDidSendAutocheckoutStatus()).Times(1);

  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  wallet_client.SendAutocheckoutStatus(autofill::CANNOT_PROCEED,
                                       GURL(kMerchantUrl),
                                       "google_transaction_id");
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(kSendAutocheckoutStatusOfFailureValidRequest, GetData(fetcher));
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(")]}'");  // Invalid JSON. Should be ignored.
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, HasRequestInProgress) {
  MockWalletClientDelegate delegate;
  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  EXPECT_FALSE(wallet_client.HasRequestInProgress());

  wallet_client.GetWalletItems(GURL(kMerchantUrl),
                               std::vector<WalletClient::RiskCapability>());
  EXPECT_TRUE(wallet_client.HasRequestInProgress());

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kGetWalletItemsValidRequest,
                         kGetWalletItemsValidResponse);
  EXPECT_FALSE(wallet_client.HasRequestInProgress());
}

TEST_F(WalletClientTest, PendingRequest) {
  MockWalletClientDelegate delegate;
  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  ASSERT_EQ(0U, wallet_client.pending_requests_.size());

  std::vector<WalletClient::RiskCapability> risk_capabilities;

  // Shouldn't queue the first request.
  wallet_client.GetWalletItems(GURL(kMerchantUrl), risk_capabilities);
  EXPECT_EQ(0U, wallet_client.pending_requests_.size());

  wallet_client.GetWalletItems(GURL(kMerchantUrl), risk_capabilities);
  EXPECT_EQ(1U, wallet_client.pending_requests_.size());

  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kGetWalletItemsValidRequest,
                         kGetWalletItemsValidResponse);
  EXPECT_EQ(0U, wallet_client.pending_requests_.size());

  EXPECT_CALL(delegate, OnWalletError(
      WalletClient::SERVICE_UNAVAILABLE)).Times(1);
  VerifyAndFinishRequest(factory,
                         net::HTTP_INTERNAL_SERVER_ERROR,
                         kGetWalletItemsValidRequest,
                         kErrorResponse);
}

TEST_F(WalletClientTest, CancelPendingRequests) {
  MockWalletClientDelegate delegate;
  net::TestURLFetcherFactory factory;

  WalletClient wallet_client(profile_.GetRequestContext(), &delegate);
  ASSERT_EQ(0U, wallet_client.pending_requests_.size());

  std::vector<WalletClient::RiskCapability> risk_capabilities;

  wallet_client.GetWalletItems(GURL(kMerchantUrl), risk_capabilities);
  wallet_client.GetWalletItems(GURL(kMerchantUrl), risk_capabilities);
  wallet_client.GetWalletItems(GURL(kMerchantUrl), risk_capabilities);
  EXPECT_EQ(2U, wallet_client.pending_requests_.size());

  wallet_client.CancelPendingRequests();
  EXPECT_EQ(0U, wallet_client.pending_requests_.size());
}

}  // namespace wallet
}  // namespace autofill
