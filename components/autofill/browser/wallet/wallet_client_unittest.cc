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

namespace autofill {
namespace wallet {

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
            "\"doc_id_1\","
            "\"doc_id_2\""
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

const char kUpdateInstrumentAddressValidRequest[] =
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

const char kUpdateInstrumentAddressWithNameChangeValidRequest[] =
    "{"
        "\"instrument_escrow_handle\":\"escrow_handle\","
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

const char kUpdateInstrumentAddressAndExpirationDateValidRequest[] =
    "{"
        "\"instrument\":"
        "{"
            "\"credit_card\":"
            "{"
                "\"exp_month\":12,"
                "\"exp_year\":2015"
            "}"
        "},"
        "\"instrument_escrow_handle\":\"escrow_handle\","
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

const char kUpdateInstrumentExpirationDateValidRequest[] =
    "{"
        "\"instrument\":"
        "{"
            "\"credit_card\":"
            "{"
                "\"exp_month\":12,"
                "\"exp_year\":2015"
            "}"
        "},"
        "\"instrument_escrow_handle\":\"escrow_handle\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"risk_params\":\"risky business\","
        "\"upgraded_instrument_id\":\"instrument_id\""
    "}";

class MockAutofillMetrics : public AutofillMetrics {
 public:
  MockAutofillMetrics() {}
  MOCK_CONST_METHOD2(LogWalletApiCallDuration,
                     void(WalletApiCallMetric metric,
                          const base::TimeDelta& duration));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillMetrics);
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

  virtual std::string GetRiskData() const OVERRIDE {
    return "risky business";
  }

  void ExpectLogWalletApiCallDuration(
      AutofillMetrics::WalletApiCallMetric metric,
      size_t times) {
    EXPECT_CALL(metric_logger_,
                LogWalletApiCallDuration(metric, testing::_)).Times(times);
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

  MockAutofillMetrics metric_logger_;
};

}  // namespace

class WalletClientTest : public testing::Test {
 public:
  WalletClientTest() : io_thread_(content::BrowserThread::IO) {}

  virtual void SetUp() OVERRIDE {
    io_thread_.StartIOThread();
    profile_.CreateRequestContext();
    wallet_client_.reset(
        new WalletClient(profile_.GetRequestContext(), &delegate_));
  }

  virtual void TearDown() OVERRIDE {
    wallet_client_.reset();
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

  void VerifyAndFinishRequest(net::HttpStatusCode response_code,
                              const std::string& request_body,
                              const std::string& response_body) {
    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    EXPECT_EQ(request_body, GetData(fetcher));
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response_body);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

 protected:
  scoped_ptr<WalletClient> wallet_client_;
  MockWalletClientDelegate delegate_;
  net::TestURLFetcherFactory factory_;

 private:
  // The profile's request context must be released on the IO thread.
  content::TestBrowserThread io_thread_;
  TestingProfile profile_;
};

TEST_F(WalletClientTest, WalletError) {
  EXPECT_CALL(delegate_, OnWalletError(
      WalletClient::SERVICE_UNAVAILABLE)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SEND_STATUS, 1);

  wallet_client_->SendAutocheckoutStatus(autofill::SUCCESS,
                                         GURL(kMerchantUrl),
                                         "google_transaction_id");
  VerifyAndFinishRequest(net::HTTP_INTERNAL_SERVER_ERROR,
                         kSendAutocheckoutStatusOfSuccessValidRequest,
                         kErrorResponse);
}

TEST_F(WalletClientTest, WalletErrorResponseMissing) {
  EXPECT_CALL(delegate_, OnWalletError(
      WalletClient::UNKNOWN_ERROR)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SEND_STATUS, 1);

  wallet_client_->SendAutocheckoutStatus(autofill::SUCCESS,
                                         GURL(kMerchantUrl),
                                         "google_transaction_id");
  VerifyAndFinishRequest(net::HTTP_INTERNAL_SERVER_ERROR,
                         kSendAutocheckoutStatusOfSuccessValidRequest,
                         kErrorTypeMissingInResponse);
}

TEST_F(WalletClientTest, NetworkFailureOnExpectedVoidResponse) {
  EXPECT_CALL(delegate_, OnNetworkError(net::HTTP_UNAUTHORIZED)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SEND_STATUS, 1);

  wallet_client_->SendAutocheckoutStatus(autofill::SUCCESS,
                                         GURL(kMerchantUrl),
                                         "");
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, NetworkFailureOnExpectedResponse) {
  EXPECT_CALL(delegate_, OnNetworkError(net::HTTP_UNAUTHORIZED)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           1);

  wallet_client_->GetWalletItems(GURL(kMerchantUrl),
                                 std::vector<WalletClient::RiskCapability>());
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, RequestError) {
  EXPECT_CALL(delegate_, OnWalletError(WalletClient::BAD_REQUEST)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SEND_STATUS, 1);

  wallet_client_->SendAutocheckoutStatus(autofill::SUCCESS,
                                         GURL(kMerchantUrl),
                                         "");
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_BAD_REQUEST);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, GetFullWalletSuccess) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_FULL_WALLET, 1);

  Cart cart("total_price", "currency_code");
  WalletClient::FullWalletRequest full_wallet_request(
      "instrument_id",
      "shipping_address_id",
      GURL(kMerchantUrl),
      cart,
      "google_transaction_id",
      std::vector<WalletClient::RiskCapability>());
  wallet_client_->GetFullWallet(full_wallet_request);

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString(
      "session_material|encrypted_one_time_pad");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kGetFullWalletValidRequest,
                         kGetFullWalletValidResponse);
  EXPECT_EQ(1U, delegate_.full_wallets_received());
}

TEST_F(WalletClientTest, GetFullWalletWithRiskCapabilitesSuccess) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_FULL_WALLET, 1);

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
  wallet_client_->GetFullWallet(full_wallet_request);

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString(
      "session_material|encrypted_one_time_pad");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kGetFullWalletWithRiskCapabilitesValidRequest,
                         kGetFullWalletValidResponse);
  EXPECT_EQ(1U, delegate_.full_wallets_received());
}

TEST_F(WalletClientTest, GetFullWalletEncryptionDown) {
  EXPECT_CALL(delegate_,
              OnNetworkError(net::HTTP_INTERNAL_SERVER_ERROR)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_FULL_WALLET, 0);

  Cart cart("total_price", "currency_code");
  WalletClient::FullWalletRequest full_wallet_request(
      "instrument_id",
      "shipping_address_id",
      GURL(kMerchantUrl),
      cart,
      "google_transaction_id",
      std::vector<WalletClient::RiskCapability>());
  wallet_client_->GetFullWallet(full_wallet_request);

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_INTERNAL_SERVER_ERROR);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  EXPECT_EQ(0U, delegate_.full_wallets_received());
}

TEST_F(WalletClientTest, GetFullWalletEncryptionMalformed) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_FULL_WALLET, 0);

  Cart cart("total_price", "currency_code");
  WalletClient::FullWalletRequest full_wallet_request(
      "instrument_id",
      "shipping_address_id",
      GURL(kMerchantUrl),
      cart,
      "google_transaction_id",
      std::vector<WalletClient::RiskCapability>());
  wallet_client_->GetFullWallet(full_wallet_request);

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString(
      "session_material:encrypted_one_time_pad");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  EXPECT_EQ(0U, delegate_.full_wallets_received());
}

TEST_F(WalletClientTest, GetFullWalletMalformedResponse) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_FULL_WALLET, 1);

  Cart cart("total_price", "currency_code");
  WalletClient::FullWalletRequest full_wallet_request(
      "instrument_id",
      "shipping_address_id",
      GURL(kMerchantUrl),
      cart,
      "google_transaction_id",
      std::vector<WalletClient::RiskCapability>());
  wallet_client_->GetFullWallet(full_wallet_request);

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString(
      "session_material|encrypted_one_time_pad");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kGetFullWalletValidRequest,
                         kGetFullWalletInvalidResponse);
  EXPECT_EQ(0U, delegate_.full_wallets_received());
}

TEST_F(WalletClientTest, AcceptLegalDocuments) {
  EXPECT_CALL(delegate_, OnDidAcceptLegalDocuments()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::ACCEPT_LEGAL_DOCUMENTS,
      1);

  ScopedVector<WalletItems::LegalDocument> docs;
  base::DictionaryValue document;
  document.SetString("legal_document_id", "doc_id_1");
  document.SetString("display_name", "doc_1");
  docs.push_back(
      WalletItems::LegalDocument::CreateLegalDocument(document).release());
  document.SetString("legal_document_id", "doc_id_2");
  document.SetString("display_name", "doc_2");
  docs.push_back(
      WalletItems::LegalDocument::CreateLegalDocument(document).release());
  docs.push_back(
      WalletItems::LegalDocument::CreatePrivacyPolicyDocument().release());
  wallet_client_->AcceptLegalDocuments(docs.get(),
                                       kGoogleTransactionId,
                                       GURL(kMerchantUrl));
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(kAcceptLegalDocumentsValidRequest, GetData(fetcher));
  fetcher->SetResponseString(")]}'");  // Invalid JSON. Should be ignored.
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(WalletClientTest, AuthenticateInstrumentSucceeded) {
  EXPECT_CALL(delegate_, OnDidAuthenticateInstrument(true)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::AUTHENTICATE_INSTRUMENT,
      1);

  wallet_client_->AuthenticateInstrument("instrument_id",
                                         "cvv",
                                         "obfuscated_gaia_id");

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kAuthenticateInstrumentValidRequest,
                         kAuthenticateInstrumentSuccessResponse);
}

TEST_F(WalletClientTest, AuthenticateInstrumentFailed) {
  EXPECT_CALL(delegate_, OnDidAuthenticateInstrument(false)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::AUTHENTICATE_INSTRUMENT,
      1);

  wallet_client_->AuthenticateInstrument("instrument_id",
                                         "cvv",
                                         "obfuscated_gaia_id");

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kAuthenticateInstrumentValidRequest,
                         kAuthenticateInstrumentFailureResponse);
}

TEST_F(WalletClientTest, AuthenticateInstrumentEscrowDown) {
  EXPECT_CALL(delegate_,
              OnNetworkError(net::HTTP_INTERNAL_SERVER_ERROR)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::AUTHENTICATE_INSTRUMENT,
      0);

  wallet_client_->AuthenticateInstrument("instrument_id",
                                         "cvv",
                                         "obfuscated_gaia_id");

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_INTERNAL_SERVER_ERROR);
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, AuthenticateInstrumentEscrowMalformed) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::AUTHENTICATE_INSTRUMENT,
      0);

  wallet_client_->AuthenticateInstrument("instrument_id",
                                         "cvv",
                                         "obfuscated_gaia_id");

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, AuthenticateInstrumentFailedMalformedResponse) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::AUTHENTICATE_INSTRUMENT,
      1);

  wallet_client_->AuthenticateInstrument("instrument_id",
                                         "cvv",
                                         "obfuscated_gaia_id");

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kAuthenticateInstrumentValidRequest,
                         kSaveInvalidResponse);
}

// TODO(ahutter): Add failure tests for GetWalletItems.

TEST_F(WalletClientTest, GetWalletItems) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           1);

  wallet_client_->GetWalletItems(GURL(kMerchantUrl),
                                 std::vector<WalletClient::RiskCapability>());

  VerifyAndFinishRequest(net::HTTP_OK,
                         kGetWalletItemsValidRequest,
                         kGetWalletItemsValidResponse);
  EXPECT_EQ(1U, delegate_.wallet_items_received());
}

TEST_F(WalletClientTest, GetWalletItemsWithRiskCapabilites) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           1);

  std::vector<WalletClient::RiskCapability> risk_capabilities;
  risk_capabilities.push_back(WalletClient::RELOGIN);

  wallet_client_->GetWalletItems(GURL(kMerchantUrl),
                                 risk_capabilities);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kGetWalletItemsWithRiskCapabilitiesValidRequest,
                         kGetWalletItemsValidResponse);
  EXPECT_EQ(1U, delegate_.wallet_items_received());
}

TEST_F(WalletClientTest, SaveAddressSucceeded) {
  EXPECT_CALL(delegate_,
              OnDidSaveAddress("saved_address_id",
                               std::vector<RequiredAction>())).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_ADDRESS, 1);

  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveAddressValidRequest,
                         kSaveAddressValidResponse);
}

TEST_F(WalletClientTest, SaveAddressWithRequiredActionsSucceeded) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_ADDRESS, 1);

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(REQUIRE_PHONE_NUMBER);
  required_actions.push_back(INVALID_FORM_FIELD);

  EXPECT_CALL(delegate_,
              OnDidSaveAddress(std::string(),
                               required_actions)).Times(1);

  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveAddressValidRequest,
                         kSaveAddressWithRequiredActionsValidResponse);
}

TEST_F(WalletClientTest, SaveAddressFailedInvalidRequiredAction) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_ADDRESS, 1);

  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveAddressValidRequest,
                         kSaveWithInvalidRequiredActionsResponse);
}

TEST_F(WalletClientTest, SaveAddressFailedMalformedResponse) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_ADDRESS, 1);

  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveAddressValidRequest,
                         kSaveInvalidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentSucceeded) {
  EXPECT_CALL(delegate_,
              OnDidSaveInstrument("instrument_id",
                                  std::vector<RequiredAction>())).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_INSTRUMENT, 1);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  wallet_client_->SaveInstrument(*instrument,
                                 "obfuscated_gaia_id",
                                 GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveInstrumentValidRequest,
                         kSaveInstrumentValidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentWithRequiredActionsSucceeded) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_INSTRUMENT, 1);

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(REQUIRE_PHONE_NUMBER);
  required_actions.push_back(INVALID_FORM_FIELD);

  EXPECT_CALL(delegate_,
              OnDidSaveInstrument(std::string(),
                                  required_actions)).Times(1);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  wallet_client_->SaveInstrument(*instrument,
                                 "obfuscated_gaia_id",
                                 GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveInstrumentValidRequest,
                         kSaveInstrumentWithRequiredActionsValidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentFailedInvalidRequiredActions) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_INSTRUMENT, 1);

  EXPECT_CALL(delegate_, OnMalformedResponse());

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  wallet_client_->SaveInstrument(*instrument,
                                 "obfuscated_gaia_id",
                                 GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveInstrumentValidRequest,
                         kSaveWithInvalidRequiredActionsResponse);
}

TEST_F(WalletClientTest, SaveInstrumentEscrowDown) {
  EXPECT_CALL(delegate_,
              OnNetworkError(net::HTTP_INTERNAL_SERVER_ERROR)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_INSTRUMENT, 0);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  wallet_client_->SaveInstrument(*instrument,
                                 "obfuscated_gaia_id",
                                 GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_INTERNAL_SERVER_ERROR);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, SaveInstrumentEscrowMalformed) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_INSTRUMENT, 0);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  wallet_client_->SaveInstrument(*instrument,
                                 "obfuscated_gaia_id",
                                 GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, SaveInstrumentFailedMalformedResponse) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_INSTRUMENT, 1);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  wallet_client_->SaveInstrument(*instrument,
                                 "obfuscated_gaia_id",
                                 GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveInstrumentValidRequest,
                         kSaveInvalidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressSucceeded) {
  EXPECT_CALL(delegate_,
              OnDidSaveInstrumentAndAddress(
                  "saved_instrument_id",
                  "saved_address_id",
                  std::vector<RequiredAction>())).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::SAVE_INSTRUMENT_AND_ADDRESS,
      1);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveInstrumentAndAddress(*instrument,
                                           *address,
                                           "obfuscated_gaia_id",
                                           GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveInstrumentAndAddressValidRequest,
                         kSaveInstrumentAndAddressValidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressWithRequiredActionsSucceeded) {
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::SAVE_INSTRUMENT_AND_ADDRESS,
      1);

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(REQUIRE_PHONE_NUMBER);
  required_actions.push_back(INVALID_FORM_FIELD);

  EXPECT_CALL(delegate_,
              OnDidSaveInstrumentAndAddress(
                  std::string(),
                  std::string(),
                  required_actions)).Times(1);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveInstrumentAndAddress(*instrument,
                                           *address,
                                           "obfuscated_gaia_id",
                                           GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
  VerifyAndFinishRequest(
      net::HTTP_OK,
      kSaveInstrumentAndAddressValidRequest,
      kSaveInstrumentAndAddressWithRequiredActionsValidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressFailedInvalidRequiredAction) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::SAVE_INSTRUMENT_AND_ADDRESS,
      1);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveInstrumentAndAddress(*instrument,
                                           *address,
                                           "obfuscated_gaia_id",
                                         GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveInstrumentAndAddressValidRequest,
                         kSaveWithInvalidRequiredActionsResponse);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressEscrowDown) {
  EXPECT_CALL(delegate_,
              OnNetworkError(net::HTTP_INTERNAL_SERVER_ERROR)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::SAVE_INSTRUMENT_AND_ADDRESS,
      0);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveInstrumentAndAddress(*instrument,
                                           *address,
                                           "obfuscated_gaia_id",
                                           GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_INTERNAL_SERVER_ERROR);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressEscrowMalformed) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::SAVE_INSTRUMENT_AND_ADDRESS,
      0);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveInstrumentAndAddress(*instrument,
                                           *address,
                                           "obfuscated_gaia_id",
                                           GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressFailedAddressMissing) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::SAVE_INSTRUMENT_AND_ADDRESS,
      1);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveInstrumentAndAddress(*instrument,
                                           *address,
                                           "obfuscated_gaia_id",
                                           GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveInstrumentAndAddressValidRequest,
                         kSaveInstrumentAndAddressMissingAddressResponse);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressFailedInstrumentMissing) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::SAVE_INSTRUMENT_AND_ADDRESS,
      1);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveInstrumentAndAddress(*instrument,
                                           *address,
                                           "obfuscated_gaia_id",
                                           GURL(kMerchantUrl));

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveInstrumentAndAddressValidRequest,
                         kSaveInstrumentAndAddressMissingInstrumentResponse);
}

TEST_F(WalletClientTest, UpdateAddressSucceeded) {
  EXPECT_CALL(delegate_,
              OnDidUpdateAddress("shipping_address_id",
                                 std::vector<RequiredAction>())).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::UPDATE_ADDRESS, 1);

  scoped_ptr<Address> address = GetTestShippingAddress();
  address->set_object_id("shipping_address_id");

  wallet_client_->UpdateAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateAddressValidRequest,
                         kUpdateAddressValidResponse);
}

TEST_F(WalletClientTest, UpdateAddressWithRequiredActionsSucceeded) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::UPDATE_ADDRESS, 1);

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(REQUIRE_PHONE_NUMBER);
  required_actions.push_back(INVALID_FORM_FIELD);

  EXPECT_CALL(delegate_,
              OnDidUpdateAddress(std::string(), required_actions)).Times(1);

  scoped_ptr<Address> address = GetTestShippingAddress();
  address->set_object_id("shipping_address_id");

  wallet_client_->UpdateAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateAddressValidRequest,
                         kUpdateWithRequiredActionsValidResponse);
}

TEST_F(WalletClientTest, UpdateAddressFailedInvalidRequiredAction) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::UPDATE_ADDRESS, 1);

  scoped_ptr<Address> address = GetTestShippingAddress();
  address->set_object_id("shipping_address_id");

  wallet_client_->UpdateAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateAddressValidRequest,
                         kSaveWithInvalidRequiredActionsResponse);
}

TEST_F(WalletClientTest, UpdateAddressMalformedResponse) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::UPDATE_ADDRESS, 1);

  scoped_ptr<Address> address = GetTestShippingAddress();
  address->set_object_id("shipping_address_id");

  wallet_client_->UpdateAddress(*address, GURL(kMerchantUrl));
  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateAddressValidRequest,
                         kUpdateMalformedResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentAddressSucceeded) {
  EXPECT_CALL(delegate_,
              OnDidUpdateInstrument("instrument_id",
                                    std::vector<RequiredAction>())).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::UPDATE_INSTRUMENT,
                                           1);

  WalletClient::UpdateInstrumentRequest update_instrument_request(
      "instrument_id",
      GURL(kMerchantUrl));

  wallet_client_->UpdateInstrument(update_instrument_request, GetTestAddress());

  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateInstrumentAddressValidRequest,
                         kUpdateInstrumentValidResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentExpirationDateSuceeded) {
  EXPECT_CALL(delegate_,
              OnDidUpdateInstrument("instrument_id",
                                    std::vector<RequiredAction>())).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::UPDATE_INSTRUMENT,
                                           1);

  WalletClient::UpdateInstrumentRequest update_instrument_request(
      "instrument_id",
      GURL(kMerchantUrl));
  update_instrument_request.expiration_month = 12;
  update_instrument_request.expiration_year = 2015;
  update_instrument_request.card_verification_number =
      "card_verification_number";
  update_instrument_request.obfuscated_gaia_id = "obfuscated_gaia_id";
  wallet_client_->UpdateInstrument(update_instrument_request,
                                   scoped_ptr<Address>());

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateInstrumentExpirationDateValidRequest,
                         kUpdateInstrumentValidResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentAddressWithNameChangeSucceeded) {
  EXPECT_CALL(delegate_,
              OnDidUpdateInstrument("instrument_id",
                                    std::vector<RequiredAction>())).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::UPDATE_INSTRUMENT,
                                           1);

  WalletClient::UpdateInstrumentRequest update_instrument_request(
      "instrument_id",
      GURL(kMerchantUrl));
  update_instrument_request.card_verification_number =
      "card_verification_number";
  update_instrument_request.obfuscated_gaia_id = "obfuscated_gaia_id";

  wallet_client_->UpdateInstrument(update_instrument_request, GetTestAddress());

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateInstrumentAddressWithNameChangeValidRequest,
                         kUpdateInstrumentValidResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentAddressAndExpirationDateSucceeded) {
  EXPECT_CALL(delegate_,
              OnDidUpdateInstrument("instrument_id",
                                    std::vector<RequiredAction>())).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::UPDATE_INSTRUMENT,
                                           1);

  WalletClient::UpdateInstrumentRequest update_instrument_request(
      "instrument_id",
      GURL(kMerchantUrl));
  update_instrument_request.expiration_month = 12;
  update_instrument_request.expiration_year = 2015;
  update_instrument_request.card_verification_number =
      "card_verification_number";
  update_instrument_request.obfuscated_gaia_id = "obfuscated_gaia_id";
  wallet_client_->UpdateInstrument(update_instrument_request, GetTestAddress());

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_OK);
  encryption_fetcher->SetResponseString("escrow_handle");
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateInstrumentAddressAndExpirationDateValidRequest,
                         kUpdateInstrumentValidResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentWithRequiredActionsSucceeded) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::UPDATE_INSTRUMENT,
                                           1);

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(REQUIRE_PHONE_NUMBER);
  required_actions.push_back(INVALID_FORM_FIELD);

  EXPECT_CALL(delegate_,
              OnDidUpdateInstrument(std::string(),
                                    required_actions)).Times(1);

  WalletClient::UpdateInstrumentRequest update_instrument_request(
      "instrument_id",
      GURL(kMerchantUrl));

  wallet_client_->UpdateInstrument(update_instrument_request, GetTestAddress());

  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateInstrumentAddressValidRequest,
                         kUpdateWithRequiredActionsValidResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentFailedInvalidRequiredAction) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::UPDATE_INSTRUMENT,
                                           1);

  WalletClient::UpdateInstrumentRequest update_instrument_request(
      "instrument_id",
      GURL(kMerchantUrl));

  wallet_client_->UpdateInstrument(update_instrument_request, GetTestAddress());

  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateInstrumentAddressValidRequest,
                         kSaveWithInvalidRequiredActionsResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentEscrowFailed) {
  EXPECT_CALL(delegate_,
              OnNetworkError(net::HTTP_INTERNAL_SERVER_ERROR)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::UPDATE_INSTRUMENT,
                                           0);

  WalletClient::UpdateInstrumentRequest update_instrument_request(
      "instrument_id",
      GURL(kMerchantUrl));
  update_instrument_request.card_verification_number =
      "card_verification_number";
  update_instrument_request.obfuscated_gaia_id = "obfuscated_gaia_id";

  wallet_client_->UpdateInstrument(update_instrument_request, GetTestAddress());

  net::TestURLFetcher* encryption_fetcher = factory_.GetFetcherByID(1);
  ASSERT_TRUE(encryption_fetcher);
  encryption_fetcher->set_response_code(net::HTTP_INTERNAL_SERVER_ERROR);
  encryption_fetcher->SetResponseString(std::string());
  encryption_fetcher->delegate()->OnURLFetchComplete(encryption_fetcher);
}

TEST_F(WalletClientTest, UpdateInstrumentMalformedResponse) {
  EXPECT_CALL(delegate_, OnMalformedResponse()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::UPDATE_INSTRUMENT,
                                           1);

  WalletClient::UpdateInstrumentRequest update_instrument_request(
      "instrument_id",
      GURL(kMerchantUrl));

  wallet_client_->UpdateInstrument(update_instrument_request, GetTestAddress());

  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateInstrumentAddressValidRequest,
                         kUpdateMalformedResponse);
}

TEST_F(WalletClientTest, SendAutocheckoutOfStatusSuccess) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SEND_STATUS, 1);

  wallet_client_->SendAutocheckoutStatus(autofill::SUCCESS,
                                         GURL(kMerchantUrl),
                                         "google_transaction_id");
  VerifyAndFinishRequest(net::HTTP_OK,
                         kSendAutocheckoutStatusOfSuccessValidRequest,
                         ")]}");  // Invalid JSON. Should be ignored.
}

TEST_F(WalletClientTest, SendAutocheckoutStatusOfFailure) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SEND_STATUS, 1);

  wallet_client_->SendAutocheckoutStatus(autofill::CANNOT_PROCEED,
                                         GURL(kMerchantUrl),
                                         "google_transaction_id");
  VerifyAndFinishRequest(net::HTTP_OK,
                         kSendAutocheckoutStatusOfFailureValidRequest,
                         ")]}");  // Invalid JSON. Should be ignored.
}

TEST_F(WalletClientTest, HasRequestInProgress) {
  EXPECT_FALSE(wallet_client_->HasRequestInProgress());
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           1);

  wallet_client_->GetWalletItems(GURL(kMerchantUrl),
                                 std::vector<WalletClient::RiskCapability>());
  EXPECT_TRUE(wallet_client_->HasRequestInProgress());

  VerifyAndFinishRequest(net::HTTP_OK,
                         kGetWalletItemsValidRequest,
                         kGetWalletItemsValidResponse);
  EXPECT_FALSE(wallet_client_->HasRequestInProgress());
}

TEST_F(WalletClientTest, PendingRequest) {
  ASSERT_EQ(0U, wallet_client_->pending_requests_.size());
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           2);

  std::vector<WalletClient::RiskCapability> risk_capabilities;

  // Shouldn't queue the first request.
  wallet_client_->GetWalletItems(GURL(kMerchantUrl), risk_capabilities);
  EXPECT_EQ(0U, wallet_client_->pending_requests_.size());

  wallet_client_->GetWalletItems(GURL(kMerchantUrl), risk_capabilities);
  EXPECT_EQ(1U, wallet_client_->pending_requests_.size());

  VerifyAndFinishRequest(net::HTTP_OK,
                         kGetWalletItemsValidRequest,
                         kGetWalletItemsValidResponse);
  EXPECT_EQ(0U, wallet_client_->pending_requests_.size());

  EXPECT_CALL(delegate_, OnWalletError(
      WalletClient::SERVICE_UNAVAILABLE)).Times(1);
  VerifyAndFinishRequest(net::HTTP_INTERNAL_SERVER_ERROR,
                         kGetWalletItemsValidRequest,
                         kErrorResponse);
}

TEST_F(WalletClientTest, CancelRequests) {
  ASSERT_EQ(0U, wallet_client_->pending_requests_.size());
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           0);

  std::vector<WalletClient::RiskCapability> risk_capabilities;

  wallet_client_->GetWalletItems(GURL(kMerchantUrl), risk_capabilities);
  wallet_client_->GetWalletItems(GURL(kMerchantUrl), risk_capabilities);
  wallet_client_->GetWalletItems(GURL(kMerchantUrl), risk_capabilities);
  EXPECT_EQ(2U, wallet_client_->pending_requests_.size());

  wallet_client_->CancelRequests();
  EXPECT_EQ(0U, wallet_client_->pending_requests_.size());
  EXPECT_FALSE(wallet_client_->HasRequestInProgress());
}

}  // namespace wallet
}  // namespace autofill
