// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/autofill/content/browser/wallet/full_wallet.h"
#include "components/autofill/content/browser/wallet/instrument.h"
#include "components/autofill/content/browser/wallet/wallet_client.h"
#include "components/autofill/content/browser/wallet/wallet_client_delegate.h"
#include "components/autofill/content/browser/wallet/wallet_items.h"
#include "components/autofill/content/browser/wallet/wallet_test_util.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace wallet {

namespace {

const char kGoogleTransactionId[] = "google-transaction-id";
const char kMerchantUrl[] = "https://example.com/path?key=value";

const char kGetFullWalletValidResponse[] =
    "{"
    "  \"expiration_month\":12,"
    "  \"expiration_year\":3000,"
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
    "      \"country_name_code\":\"US\","
    "      \"language_code\":\"language_code\""
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
    "      \"country_name_code\":\"US\","
    "      \"language_code\":\"ship_language_code\""
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
    "      \"last_four_digits\":\"4111\","
    "      \"expiration_month\":12,"
    "      \"expiration_year\":3000,"
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
    "        \"country_code\":\"US\","
    "        \"language_code\":\"language_code\""
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
    "  \"form_field_error\":"
    "  ["
    "    {"
    "      \"location\":\"SHIPPING_ADDRESS\","
    "      \"type\":\"INVALID_POSTAL_CODE\""
    "    }"
    "  ],"
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
    "  \"form_field_error\":"
    "  ["
    "    {"
    "      \"location\":\"SHIPPING_ADDRESS\","
    "      \"type\":\"INVALID_POSTAL_CODE\""
    "    }"
    "  ],"
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
    "  \"form_field_error\":"
    "  ["
    "    {"
    "      \"location\":\"SHIPPING_ADDRESS\","
    "      \"type\":\"INVALID_POSTAL_CODE\""
    "    }"
    "  ],"
    "  \"required_action\":"
    "  ["
    "    \"  \\treqUIRE_PhOnE_number   \\n\\r\","
    "    \"INVALID_form_field\""
    "  ]"
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
    "  \"form_field_error\":"
    "  ["
    "    {"
    "      \"location\":\"SHIPPING_ADDRESS\","
    "      \"type\":\"INVALID_POSTAL_CODE\""
    "    }"
    "  ],"
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

const char kErrorResponseSpendingLimitExceeded[] =
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
    "    \"error_type\":\"SPENDING_LIMIT_EXCEEDED\","
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
// JSON from DictionaryValues. NB: The values must be alphabetical to pass
// the tests.

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
        "\"instrument_id\":\"instrument_id\","
        "\"risk_params\":\"risky business\""
    "}";

const char kGetFullWalletValidRequest[] =
    "{"
        "\"feature\":\"REQUEST_AUTOCOMPLETE\","
        "\"google_transaction_id\":\"google_transaction_id\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"new_wallet_user\":false,"
        "\"phone_number_required\":true,"
        "\"risk_params\":\"risky business\","
        "\"selected_address_id\":\"shipping_address_id\","
        "\"selected_instrument_id\":\"instrument_id\","
        "\"supported_risk_challenge\":"
        "["
        "],"
        "\"use_minimal_addresses\":false"
    "}";

const char kGetFullWalletValidRequestNewUser[] =
    "{"
        "\"feature\":\"REQUEST_AUTOCOMPLETE\","
        "\"google_transaction_id\":\"google_transaction_id\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"new_wallet_user\":true,"
        "\"phone_number_required\":true,"
        "\"risk_params\":\"risky business\","
        "\"selected_address_id\":\"shipping_address_id\","
        "\"selected_instrument_id\":\"instrument_id\","
        "\"supported_risk_challenge\":"
        "["
        "],"
        "\"use_minimal_addresses\":false"
    "}";

const char kGetFullWalletWithRiskCapabilitesValidRequest[] =
    "{"
        "\"feature\":\"REQUEST_AUTOCOMPLETE\","
        "\"google_transaction_id\":\"google_transaction_id\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"new_wallet_user\":false,"
        "\"phone_number_required\":true,"
        "\"risk_params\":\"risky business\","
        "\"selected_address_id\":\"shipping_address_id\","
        "\"selected_instrument_id\":\"instrument_id\","
        "\"supported_risk_challenge\":"
        "["
            "\"VERIFY_CVC\""
        "],"
        "\"use_minimal_addresses\":false"
    "}";

const char kGetWalletItemsValidRequest[] =
    "{"
        "\"merchant_domain\":\"https://example.com/\","
        "\"phone_number_required\":true,"
        "\"shipping_address_required\":true,"
        "\"use_minimal_addresses\":false"
    "}";

const char kGetWalletItemsWithTransactionDetails[] =
    "{"
        "\"currency_code\":\"USD\","
        "\"estimated_total_price\":\"100.00\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"phone_number_required\":true,"
        "\"shipping_address_required\":true,"
        "\"use_minimal_addresses\":false"
    "}";

const char kGetWalletItemsNoShippingRequest[] =
    "{"
        "\"merchant_domain\":\"https://example.com/\","
        "\"phone_number_required\":true,"
        "\"shipping_address_required\":false,"
        "\"use_minimal_addresses\":false"
    "}";

const char kSaveAddressValidRequest[] =
    "{"
        "\"merchant_domain\":\"https://example.com/\","
        "\"phone_number_required\":true,"
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
                "\"country_name_code\":\"US\","
                "\"dependent_locality_name\":\"save_dependent_locality_name\","
                "\"language_code\":\"save_language_code\","
                "\"locality_name\":\"save_locality_name\","
                "\"postal_code_number\":\"save_postal_code_number\","
                "\"recipient_name\":\"save_recipient_name\","
                "\"sorting_code\":\"save_sorting_code\""
            "}"
        "},"
        "\"use_minimal_addresses\":false"
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
                    "\"country_name_code\":\"US\","
                    "\"dependent_locality_name\":\"dependent_locality_name\","
                    "\"language_code\":\"language_code\","
                    "\"locality_name\":\"locality_name\","
                    "\"postal_code_number\":\"postal_code_number\","
                    "\"recipient_name\":\"recipient_name\","
                    "\"sorting_code\":\"sorting_code\""
                "},"
                "\"exp_month\":12,"
                "\"exp_year\":3000,"
                "\"fop_type\":\"VISA\","
                "\"last_4_digits\":\"4448\""
            "},"
            "\"type\":\"CREDIT_CARD\""
        "},"
        "\"instrument_phone_number\":\"phone_number\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"phone_number_required\":true,"
        "\"risk_params\":\"risky business\","
        "\"use_minimal_addresses\":false"
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
                    "\"country_name_code\":\"US\","
                    "\"dependent_locality_name\":\"dependent_locality_name\","
                    "\"language_code\":\"language_code\","
                    "\"locality_name\":\"locality_name\","
                    "\"postal_code_number\":\"postal_code_number\","
                    "\"recipient_name\":\"recipient_name\","
                    "\"sorting_code\":\"sorting_code\""
                "},"
                "\"exp_month\":12,"
                "\"exp_year\":3000,"
                "\"fop_type\":\"VISA\","
                "\"last_4_digits\":\"4448\""
            "},"
            "\"type\":\"CREDIT_CARD\""
        "},"
        "\"instrument_phone_number\":\"phone_number\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"phone_number_required\":true,"
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
                "\"country_name_code\":\"US\","
                "\"dependent_locality_name\":\"save_dependent_locality_name\","
                "\"language_code\":\"save_language_code\","
                "\"locality_name\":\"save_locality_name\","
                "\"postal_code_number\":\"save_postal_code_number\","
                "\"recipient_name\":\"save_recipient_name\","
                "\"sorting_code\":\"save_sorting_code\""
            "}"
        "},"
        "\"use_minimal_addresses\":false"
    "}";

const char kUpdateAddressValidRequest[] =
    "{"
        "\"merchant_domain\":\"https://example.com/\","
        "\"phone_number_required\":true,"
        "\"risk_params\":\"risky business\","
        "\"shipping_address\":"
        "{"
            "\"id\":\"address_id\","
            "\"phone_number\":\"ship_phone_number\","
            "\"postal_address\":"
            "{"
                "\"address_line\":"
                "["
                    "\"ship_address_line_1\","
                    "\"ship_address_line_2\""
                "],"
                "\"administrative_area_name\":\"ship_admin_area_name\","
                "\"country_name_code\":\"US\","
                "\"dependent_locality_name\":\"ship_dependent_locality_name\","
                "\"language_code\":\"ship_language_code\","
                "\"locality_name\":\"ship_locality_name\","
                "\"postal_code_number\":\"ship_postal_code_number\","
                "\"recipient_name\":\"ship_recipient_name\","
                "\"sorting_code\":\"ship_sorting_code\""
            "}"
        "},"
        "\"use_minimal_addresses\":false"
    "}";

const char kUpdateInstrumentAddressValidRequest[] =
    "{"
        "\"instrument_phone_number\":\"phone_number\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"phone_number_required\":true,"
        "\"risk_params\":\"risky business\","
        "\"upgraded_billing_address\":"
        "{"
            "\"address_line\":"
            "["
                "\"address_line_1\","
                "\"address_line_2\""
            "],"
            "\"administrative_area_name\":\"admin_area_name\","
            "\"country_name_code\":\"US\","
            "\"dependent_locality_name\":\"dependent_locality_name\","
            "\"language_code\":\"language_code\","
            "\"locality_name\":\"locality_name\","
            "\"postal_code_number\":\"postal_code_number\","
            "\"recipient_name\":\"recipient_name\","
            "\"sorting_code\":\"sorting_code\""
        "},"
        "\"upgraded_instrument_id\":\"default_instrument_id\","
        "\"use_minimal_addresses\":false"
    "}";

const char kUpdateInstrumentAddressWithNameChangeValidRequest[] =
    "{"
        "\"instrument_phone_number\":\"phone_number\","
        "\"merchant_domain\":\"https://example.com/\","
        "\"phone_number_required\":true,"
        "\"risk_params\":\"risky business\","
        "\"upgraded_billing_address\":"
        "{"
            "\"address_line\":"
            "["
                "\"address_line_1\","
                "\"address_line_2\""
            "],"
            "\"administrative_area_name\":\"admin_area_name\","
            "\"country_name_code\":\"US\","
            "\"dependent_locality_name\":\"dependent_locality_name\","
            "\"language_code\":\"language_code\","
            "\"locality_name\":\"locality_name\","
            "\"postal_code_number\":\"postal_code_number\","
            "\"recipient_name\":\"recipient_name\","
            "\"sorting_code\":\"sorting_code\""
        "},"
        "\"upgraded_instrument_id\":\"default_instrument_id\","
        "\"use_minimal_addresses\":false"
    "}";

const char kUpdateInstrumentExpirationDateValidRequest[] =
    "{"
        "\"instrument\":"
        "{"
            "\"credit_card\":"
            "{"
                "\"exp_month\":12,"
                "\"exp_year\":3001"
            "},"
            "\"type\":\"CREDIT_CARD\""
        "},"
        "\"merchant_domain\":\"https://example.com/\","
        "\"phone_number_required\":true,"
        "\"risk_params\":\"risky business\","
        "\"upgraded_instrument_id\":\"instrument_id\","
        "\"use_minimal_addresses\":false"
    "}";

class MockAutofillMetrics : public AutofillMetrics {
 public:
  MockAutofillMetrics() {}
  MOCK_CONST_METHOD2(LogWalletApiCallDuration,
                     void(WalletApiCallMetric metric,
                          const base::TimeDelta& duration));
  MOCK_CONST_METHOD1(LogWalletErrorMetric, void(WalletErrorMetric metric));
  MOCK_CONST_METHOD1(LogWalletRequiredActionMetric,
                     void(WalletRequiredActionMetric action));
  MOCK_CONST_METHOD1(LogWalletMalformedResponseMetric,
                     void(WalletApiCallMetric metric));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillMetrics);
};

class MockWalletClientDelegate : public WalletClientDelegate {
 public:
  MockWalletClientDelegate()
      : full_wallets_received_(0),
        wallet_items_received_(0),
        is_shipping_required_(true) {}
  ~MockWalletClientDelegate() {}

  virtual const AutofillMetrics& GetMetricLogger() const OVERRIDE {
    return metric_logger_;
  }

  virtual std::string GetRiskData() const OVERRIDE {
    return "risky business";
  }

  virtual std::string GetWalletCookieValue() const OVERRIDE {
    return "gdToken";
  }

  virtual bool IsShippingAddressRequired() const OVERRIDE {
    return is_shipping_required_;
  }

  void SetIsShippingAddressRequired(bool is_shipping_required) {
    is_shipping_required_ = is_shipping_required;
  }

  void ExpectLogWalletApiCallDuration(
      AutofillMetrics::WalletApiCallMetric metric,
      size_t times) {
    EXPECT_CALL(metric_logger_,
                LogWalletApiCallDuration(metric, testing::_)).Times(times);
  }

  void ExpectLogWalletMalformedResponse(
      AutofillMetrics::WalletApiCallMetric metric) {
    EXPECT_CALL(metric_logger_,
                LogWalletMalformedResponseMetric(metric)).Times(1);
  }

  void ExpectWalletErrorMetric(AutofillMetrics::WalletErrorMetric metric) {
    EXPECT_CALL(metric_logger_, LogWalletErrorMetric(metric)).Times(1);
  }

  void ExpectWalletRequiredActionMetric(
      AutofillMetrics::WalletRequiredActionMetric metric) {
    EXPECT_CALL(metric_logger_,
                LogWalletRequiredActionMetric(metric)).Times(1);
  }

  void ExpectBaselineMetrics() {
    EXPECT_CALL(
        metric_logger_,
        LogWalletErrorMetric(
            AutofillMetrics::WALLET_ERROR_BASELINE_ISSUED_REQUEST))
                .Times(1);
    ExpectWalletRequiredActionMetric(
        AutofillMetrics::WALLET_REQUIRED_ACTION_BASELINE_ISSUED_REQUEST);
  }

  MockAutofillMetrics* metric_logger() {
    return &metric_logger_;
  }

  MOCK_METHOD0(OnDidAcceptLegalDocuments, void());
  MOCK_METHOD1(OnDidAuthenticateInstrument, void(bool success));
  MOCK_METHOD4(OnDidSaveToWallet,
               void(const std::string& instrument_id,
                    const std::string& shipping_address_id,
                    const std::vector<RequiredAction>& required_actions,
                    const std::vector<FormFieldError>& form_field_errors));
  MOCK_METHOD1(OnWalletError, void(WalletClient::ErrorType error_type));

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
  bool is_shipping_required_;

  testing::StrictMock<MockAutofillMetrics> metric_logger_;
};

}  // namespace

class WalletClientTest : public testing::Test {
 public:
  WalletClientTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::MessageLoopProxy::current())) {}
  virtual ~WalletClientTest() {}

  virtual void SetUp() OVERRIDE {
    wallet_client_.reset(new WalletClient(
        request_context_.get(), &delegate_, GURL(kMerchantUrl)));
  }

  virtual void TearDown() OVERRIDE {
    wallet_client_.reset();
  }

  void VerifyAndFinishRequest(net::HttpStatusCode response_code,
                              const std::string& request_body,
                              const std::string& response_body) {
    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);

    const std::string& upload_data = fetcher->upload_data();
    EXPECT_EQ(request_body, GetData(upload_data));
    net::HttpRequestHeaders request_headers;
    fetcher->GetExtraRequestHeaders(&request_headers);
    std::string auth_header_value;
    EXPECT_TRUE(request_headers.GetHeader(
        net::HttpRequestHeaders::kAuthorization,
        &auth_header_value));
    EXPECT_EQ("GoogleLogin auth=gdToken", auth_header_value);

    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response_body);
    fetcher->delegate()->OnURLFetchComplete(fetcher);

    // Pump the message loop to catch up to any asynchronous tasks that might
    // have been posted from OnURLFetchComplete().
    base::RunLoop().RunUntilIdle();
  }

  void VerifyAndFinishFormEncodedRequest(net::HttpStatusCode response_code,
                                         const std::string& json_payload,
                                         const std::string& response_body,
                                         size_t expected_parameter_number) {
    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);

    net::HttpRequestHeaders request_headers;
    fetcher->GetExtraRequestHeaders(&request_headers);
    std::string auth_header_value;
    EXPECT_TRUE(request_headers.GetHeader(
        net::HttpRequestHeaders::kAuthorization,
        &auth_header_value));
    EXPECT_EQ("GoogleLogin auth=gdToken", auth_header_value);

    const std::string& upload_data = fetcher->upload_data();
    base::StringPairs tokens;
    base::SplitStringIntoKeyValuePairs(upload_data, '=', '&', &tokens);
    EXPECT_EQ(tokens.size(), expected_parameter_number);

    size_t num_params = 0U;
    for (base::StringPairs::const_iterator iter = tokens.begin();
         iter != tokens.end();
         ++iter) {
      const std::string& key = iter->first;
      const std::string& value = iter->second;

      if (key == "request_content_type") {
        EXPECT_EQ("application/json", value);
        num_params++;
      }

      if (key == "request") {
        EXPECT_EQ(json_payload,
                  GetData(
                      net::UnescapeURLComponent(
                          value, net::UnescapeRule::URL_SPECIAL_CHARS |
                          net::UnescapeRule::REPLACE_PLUS_WITH_SPACE)));
        num_params++;
      }

      if (key == "cvn") {
        EXPECT_EQ("123", value);
        num_params++;
      }

      if (key == "card_number") {
        EXPECT_EQ("4444444444444448", value);
        num_params++;
      }

      if (key == "otp") {
        EXPECT_FALSE(value.empty());
        num_params++;
      }
    }
    EXPECT_EQ(expected_parameter_number, num_params);

    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response_body);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void TestWalletErrorCode(
      const std::string& error_type_string,
      const std::string& message_type_for_buyer_string,
      WalletClient::ErrorType expected_error_type,
      AutofillMetrics::WalletErrorMetric expected_autofill_metric) {
    static const char kResponseTemplate[] =
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
        "    \"error_type\":\"%s\","
        "    %s"  // Placeholder for |user_error_type|.
        "    \"error_detail\":\"error_detail\","
        "    \"message_for_user\":"
        "    {"
        "      \"text\":\"text\","
        "      \"subtext\":\"subtext\","
        "      \"details\":\"details\""
        "    }"
        "  }"
        "}";
    EXPECT_CALL(delegate_, OnWalletError(expected_error_type)).Times(1);
    delegate_.ExpectLogWalletApiCallDuration(
        AutofillMetrics::GET_WALLET_ITEMS, 1);
    delegate_.ExpectBaselineMetrics();
    delegate_.ExpectWalletErrorMetric(expected_autofill_metric);

    wallet_client_->GetWalletItems(base::string16(), base::string16());
    std::string buyer_error;
    if (!message_type_for_buyer_string.empty()) {
      buyer_error = base::StringPrintf("\"message_type_for_buyer\":\"%s\",",
                                       message_type_for_buyer_string.c_str());
    }
    std::string response = base::StringPrintf(kResponseTemplate,
                                              error_type_string.c_str(),
                                              buyer_error.c_str());
    VerifyAndFinishRequest(net::HTTP_INTERNAL_SERVER_ERROR,
                           kGetWalletItemsValidRequest,
                           response);
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<WalletClient> wallet_client_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  MockWalletClientDelegate delegate_;

 private:
  std::string GetData(const std::string& upload_data) {
    scoped_ptr<base::Value> root(base::JSONReader::Read(upload_data));

    // If this is not a JSON dictionary, return plain text.
    if (!root || !root->IsType(base::Value::TYPE_DICTIONARY))
      return upload_data;

    // Remove api_key entry (to prevent accidental leak), return JSON as text.
    base::DictionaryValue* dict =
        static_cast<base::DictionaryValue*>(root.get());
    dict->Remove("api_key", NULL);
    std::string clean_upload_data;
    base::JSONWriter::Write(dict, &clean_upload_data);
    return clean_upload_data;
  }

  net::TestURLFetcherFactory factory_;
};

TEST_F(WalletClientTest, WalletErrorCodes) {
  struct {
    std::string error_type_string;
    std::string message_type_for_buyer_string;
    WalletClient::ErrorType expected_error_type;
    AutofillMetrics::WalletErrorMetric expected_autofill_metric;
  } test_cases[] = {
      // General |BUYER_ACCOUNT_ERROR| with no |message_type_for_buyer_string|.
      {
          "buyer_account_error",
          "",
          WalletClient::BUYER_ACCOUNT_ERROR,
          AutofillMetrics::WALLET_BUYER_ACCOUNT_ERROR
      },
      // |BUYER_ACCOUNT_ERROR| with "buyer_legal_address_not_supported" in
      // message_type_for_buyer field.
      {
          "buyer_account_error",
          "bla_country_not_supported",
          WalletClient::BUYER_LEGAL_ADDRESS_NOT_SUPPORTED,
          AutofillMetrics::WALLET_BUYER_LEGAL_ADDRESS_NOT_SUPPORTED
      },
      // |BUYER_ACCOUNT_ERROR| with KYC error code in message_type_for_buyer
      // field.
      {
          "buyer_account_error",
          "buyer_kyc_error",
          WalletClient::UNVERIFIED_KNOW_YOUR_CUSTOMER_STATUS,
          AutofillMetrics::WALLET_UNVERIFIED_KNOW_YOUR_CUSTOMER_STATUS
      },
      // |BUYER_ACCOUNT_ERROR| with un-recognizable |message_type_for_buyer|.
      {
          "buyer_account_error",
          "random_string",
          WalletClient::BUYER_ACCOUNT_ERROR,
          AutofillMetrics::WALLET_BUYER_ACCOUNT_ERROR
      },
      // The following are other error types we could get from Wallet.
      {
          "unsupported_merchant",
          "",
          WalletClient::UNSUPPORTED_MERCHANT,
          AutofillMetrics::WALLET_UNSUPPORTED_MERCHANT
      },
      {
          "internal_error",
          "",
          WalletClient::INTERNAL_ERROR,
          AutofillMetrics::WALLET_INTERNAL_ERROR
      },
      {
          "invalid_params",
          "",
          WalletClient::INVALID_PARAMS,
          AutofillMetrics::WALLET_INVALID_PARAMS
      },
      {
          "service_unavailable",
          "",
          WalletClient::SERVICE_UNAVAILABLE,
          AutofillMetrics::WALLET_SERVICE_UNAVAILABLE
      },
      {
          "unsupported_api_version",
          "",
          WalletClient::UNSUPPORTED_API_VERSION,
          AutofillMetrics::WALLET_UNSUPPORTED_API_VERSION
      },
      // Any un-recognizable |error_type| is a |UNKNOWN_ERROR|.
      {
          "random_string_1",
          "",
          WalletClient::UNKNOWN_ERROR,
          AutofillMetrics::WALLET_UNKNOWN_ERROR
      },
      {
          "random_string_2",
          "",
          WalletClient::UNKNOWN_ERROR,
          AutofillMetrics::WALLET_UNKNOWN_ERROR
      },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    SCOPED_TRACE(
        base::StringPrintf(
            "%s - %s",
            test_cases[i].error_type_string.c_str(),
            test_cases[i].message_type_for_buyer_string.c_str()));
    TestWalletErrorCode(test_cases[i].error_type_string,
                        test_cases[i].message_type_for_buyer_string,
                        test_cases[i].expected_error_type,
                        test_cases[i].expected_autofill_metric);
  }
}


TEST_F(WalletClientTest, WalletErrorResponseMissing) {
  EXPECT_CALL(delegate_, OnWalletError(
      WalletClient::UNKNOWN_ERROR)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::GET_WALLET_ITEMS, 1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_UNKNOWN_ERROR);

  wallet_client_->GetWalletItems(base::string16(), base::string16());
  VerifyAndFinishRequest(net::HTTP_INTERNAL_SERVER_ERROR,
                         kGetWalletItemsValidRequest,
                         kErrorTypeMissingInResponse);
}

TEST_F(WalletClientTest, NetworkFailureOnExpectedResponse) {
  EXPECT_CALL(delegate_, OnWalletError(WalletClient::NETWORK_ERROR)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_NETWORK_ERROR);

  wallet_client_->GetWalletItems(base::string16(), base::string16());
  VerifyAndFinishRequest(net::HTTP_UNAUTHORIZED,
                         kGetWalletItemsValidRequest,
                         std::string());
}

TEST_F(WalletClientTest, RequestError) {
  EXPECT_CALL(delegate_, OnWalletError(WalletClient::BAD_REQUEST)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::GET_WALLET_ITEMS, 1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_BAD_REQUEST);

  wallet_client_->GetWalletItems(base::string16(), base::string16());
  VerifyAndFinishRequest(net::HTTP_BAD_REQUEST,
                         kGetWalletItemsValidRequest,
                         std::string());
}

TEST_F(WalletClientTest, GetFullWalletSuccess) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_FULL_WALLET, 1);
  delegate_.ExpectBaselineMetrics();

  WalletClient::FullWalletRequest full_wallet_request(
      "instrument_id",
      "shipping_address_id",
      "google_transaction_id",
      std::vector<WalletClient::RiskCapability>(),
      false);
  wallet_client_->GetFullWallet(full_wallet_request);

  VerifyAndFinishFormEncodedRequest(net::HTTP_OK,
                                    kGetFullWalletValidRequest,
                                    kGetFullWalletValidResponse,
                                    3U);
  EXPECT_EQ(1U, delegate_.full_wallets_received());
}

TEST_F(WalletClientTest, GetFullWalletSuccessNewuser) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_FULL_WALLET, 1);
  delegate_.ExpectBaselineMetrics();

  WalletClient::FullWalletRequest full_wallet_request(
      "instrument_id",
      "shipping_address_id",
      "google_transaction_id",
      std::vector<WalletClient::RiskCapability>(),
      true);
  wallet_client_->GetFullWallet(full_wallet_request);

  VerifyAndFinishFormEncodedRequest(net::HTTP_OK,
                                    kGetFullWalletValidRequestNewUser,
                                    kGetFullWalletValidResponse,
                                    3U);
  EXPECT_EQ(1U, delegate_.full_wallets_received());
}

TEST_F(WalletClientTest, GetFullWalletWithRiskCapabilitesSuccess) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_FULL_WALLET, 1);
  delegate_.ExpectBaselineMetrics();

  std::vector<WalletClient::RiskCapability> risk_capabilities;
  risk_capabilities.push_back(WalletClient::VERIFY_CVC);
  WalletClient::FullWalletRequest full_wallet_request(
      "instrument_id",
      "shipping_address_id",
      "google_transaction_id",
      risk_capabilities,
      false);
  wallet_client_->GetFullWallet(full_wallet_request);

  VerifyAndFinishFormEncodedRequest(
      net::HTTP_OK,
      kGetFullWalletWithRiskCapabilitesValidRequest,
      kGetFullWalletValidResponse,
      3U);
  EXPECT_EQ(1U, delegate_.full_wallets_received());
}


TEST_F(WalletClientTest, GetFullWalletMalformedResponse) {
  EXPECT_CALL(delegate_,
              OnWalletError(WalletClient::MALFORMED_RESPONSE)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_FULL_WALLET, 1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_MALFORMED_RESPONSE);
  delegate_.ExpectLogWalletMalformedResponse(AutofillMetrics::GET_FULL_WALLET);

  WalletClient::FullWalletRequest full_wallet_request(
      "instrument_id",
      "shipping_address_id",
      "google_transaction_id",
      std::vector<WalletClient::RiskCapability>(),
      false);
  wallet_client_->GetFullWallet(full_wallet_request);

  VerifyAndFinishFormEncodedRequest(net::HTTP_OK,
                                    kGetFullWalletValidRequest,
                                    kGetFullWalletInvalidResponse,
                                    3U);
  EXPECT_EQ(0U, delegate_.full_wallets_received());
}

TEST_F(WalletClientTest, AcceptLegalDocuments) {
  EXPECT_CALL(delegate_, OnDidAcceptLegalDocuments()).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::ACCEPT_LEGAL_DOCUMENTS,
      1);
  delegate_.ExpectBaselineMetrics();

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
                                       kGoogleTransactionId);
  VerifyAndFinishRequest(net::HTTP_OK,
                         kAcceptLegalDocumentsValidRequest,
                         ")}'");  // Invalid JSON. Should be ignored.
}

TEST_F(WalletClientTest, AuthenticateInstrumentSucceeded) {
  EXPECT_CALL(delegate_, OnDidAuthenticateInstrument(true)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::AUTHENTICATE_INSTRUMENT,
      1);
  delegate_.ExpectBaselineMetrics();

  wallet_client_->AuthenticateInstrument("instrument_id", "123");

  VerifyAndFinishFormEncodedRequest(net::HTTP_OK,
                                    kAuthenticateInstrumentValidRequest,
                                    kAuthenticateInstrumentSuccessResponse,
                                    3U);
}

TEST_F(WalletClientTest, AuthenticateInstrumentFailed) {
  EXPECT_CALL(delegate_, OnDidAuthenticateInstrument(false)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::AUTHENTICATE_INSTRUMENT,
      1);
  delegate_.ExpectBaselineMetrics();

  wallet_client_->AuthenticateInstrument("instrument_id", "123");

  VerifyAndFinishFormEncodedRequest(net::HTTP_OK,
                                    kAuthenticateInstrumentValidRequest,
                                    kAuthenticateInstrumentFailureResponse,
                                    3U);
}

TEST_F(WalletClientTest, AuthenticateInstrumentFailedMalformedResponse) {
  EXPECT_CALL(delegate_,
              OnWalletError(WalletClient::MALFORMED_RESPONSE)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::AUTHENTICATE_INSTRUMENT,
      1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_MALFORMED_RESPONSE);
  delegate_.ExpectLogWalletMalformedResponse(
      AutofillMetrics::AUTHENTICATE_INSTRUMENT);

  wallet_client_->AuthenticateInstrument("instrument_id", "123");

  VerifyAndFinishFormEncodedRequest(net::HTTP_OK,
                                    kAuthenticateInstrumentValidRequest,
                                    kSaveInvalidResponse,
                                    3U);
}

// TODO(ahutter): Add failure tests for GetWalletItems.

TEST_F(WalletClientTest, GetWalletItems) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           1);
  delegate_.ExpectBaselineMetrics();

  wallet_client_->GetWalletItems(base::string16(), base::string16());

  VerifyAndFinishRequest(net::HTTP_OK,
                         kGetWalletItemsValidRequest,
                         kGetWalletItemsValidResponse);
  EXPECT_EQ(1U, delegate_.wallet_items_received());
}

TEST_F(WalletClientTest, GetWalletItemsWithTransactionDetails) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           1);
  delegate_.ExpectBaselineMetrics();

  wallet_client_->GetWalletItems(base::ASCIIToUTF16("100.00"),
                                 base::ASCIIToUTF16("USD"));

  VerifyAndFinishRequest(net::HTTP_OK,
                         kGetWalletItemsWithTransactionDetails,
                         kGetWalletItemsValidResponse);
  EXPECT_EQ(1U, delegate_.wallet_items_received());
}

TEST_F(WalletClientTest, GetWalletItemsRespectsDelegateForShippingRequired) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           1);
  delegate_.ExpectBaselineMetrics();
  delegate_.SetIsShippingAddressRequired(false);

  wallet_client_->GetWalletItems(base::string16(), base::string16());

  VerifyAndFinishRequest(net::HTTP_OK,
                         kGetWalletItemsNoShippingRequest,
                         kGetWalletItemsValidResponse);
  EXPECT_EQ(1U, delegate_.wallet_items_received());
}

TEST_F(WalletClientTest, SaveAddressSucceeded) {
  EXPECT_CALL(delegate_,
              OnDidSaveToWallet(std::string(),
                                "saved_address_id",
                                std::vector<RequiredAction>(),
                                std::vector<FormFieldError>())).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET, 1);
  delegate_.ExpectBaselineMetrics();

  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveToWallet(scoped_ptr<Instrument>(),
                               address.Pass(),
                               NULL,
                               NULL);
  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveAddressValidRequest,
                         kSaveAddressValidResponse);
}

TEST_F(WalletClientTest, SaveAddressWithRequiredActionsSucceeded) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET, 1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletRequiredActionMetric(
      AutofillMetrics::REQUIRE_PHONE_NUMBER);
  delegate_.ExpectWalletRequiredActionMetric(
      AutofillMetrics::INVALID_FORM_FIELD);

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(REQUIRE_PHONE_NUMBER);
  required_actions.push_back(INVALID_FORM_FIELD);

  std::vector<FormFieldError> form_errors;
  form_errors.push_back(FormFieldError(FormFieldError::INVALID_POSTAL_CODE,
                                       FormFieldError::SHIPPING_ADDRESS));

  EXPECT_CALL(delegate_,
              OnDidSaveToWallet(std::string(),
                                std::string(),
                                required_actions,
                                form_errors)).Times(1);

  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveToWallet(scoped_ptr<Instrument>(),
                               address.Pass(),
                               NULL,
                               NULL);
  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveAddressValidRequest,
                         kSaveAddressWithRequiredActionsValidResponse);
}

TEST_F(WalletClientTest, SaveAddressFailedInvalidRequiredAction) {
  EXPECT_CALL(delegate_,
              OnWalletError(WalletClient::MALFORMED_RESPONSE)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET, 1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_MALFORMED_RESPONSE);
  delegate_.ExpectLogWalletMalformedResponse(AutofillMetrics::SAVE_TO_WALLET);

  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveToWallet(scoped_ptr<Instrument>(),
                               address.Pass(),
                               NULL,
                               NULL);
  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveAddressValidRequest,
                         kSaveWithInvalidRequiredActionsResponse);
}

TEST_F(WalletClientTest, SaveAddressFailedMalformedResponse) {
  EXPECT_CALL(delegate_,
              OnWalletError(WalletClient::MALFORMED_RESPONSE)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET, 1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_MALFORMED_RESPONSE);
  delegate_.ExpectLogWalletMalformedResponse(AutofillMetrics::SAVE_TO_WALLET);

  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveToWallet(scoped_ptr<Instrument>(),
                               address.Pass(),
                               NULL,
                               NULL);
  VerifyAndFinishRequest(net::HTTP_OK,
                         kSaveAddressValidRequest,
                         kSaveInvalidResponse);
}

TEST_F(WalletClientTest, SaveInstrumentSucceeded) {
  EXPECT_CALL(delegate_,
              OnDidSaveToWallet("instrument_id",
                                std::string(),
                                std::vector<RequiredAction>(),
                                std::vector<FormFieldError>())).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET, 1);
  delegate_.ExpectBaselineMetrics();

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  wallet_client_->SaveToWallet(instrument.Pass(),
                               scoped_ptr<Address>(),
                               NULL,
                               NULL);

  VerifyAndFinishFormEncodedRequest(net::HTTP_OK,
                                    kSaveInstrumentValidRequest,
                                    kSaveInstrumentValidResponse,
                                    4U);
}

TEST_F(WalletClientTest, SaveInstrumentWithRequiredActionsSucceeded) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET, 1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletRequiredActionMetric(
      AutofillMetrics::REQUIRE_PHONE_NUMBER);
  delegate_.ExpectWalletRequiredActionMetric(
      AutofillMetrics::INVALID_FORM_FIELD);

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(REQUIRE_PHONE_NUMBER);
  required_actions.push_back(INVALID_FORM_FIELD);

  std::vector<FormFieldError> form_errors;
  form_errors.push_back(FormFieldError(FormFieldError::INVALID_POSTAL_CODE,
                                       FormFieldError::SHIPPING_ADDRESS));

  EXPECT_CALL(delegate_,
              OnDidSaveToWallet(std::string(),
                                std::string(),
                                required_actions,
                                form_errors)).Times(1);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  wallet_client_->SaveToWallet(instrument.Pass(),
                               scoped_ptr<Address>(),
                               NULL,
                               NULL);

  VerifyAndFinishFormEncodedRequest(
      net::HTTP_OK,
      kSaveInstrumentValidRequest,
      kSaveInstrumentWithRequiredActionsValidResponse,
      4U);
}

TEST_F(WalletClientTest, SaveInstrumentFailedInvalidRequiredActions) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET, 1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_MALFORMED_RESPONSE);
  delegate_.ExpectLogWalletMalformedResponse(AutofillMetrics::SAVE_TO_WALLET);

  EXPECT_CALL(delegate_,
              OnWalletError(WalletClient::MALFORMED_RESPONSE));

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  wallet_client_->SaveToWallet(instrument.Pass(),
                               scoped_ptr<Address>(),
                               NULL,
                               NULL);

  VerifyAndFinishFormEncodedRequest(net::HTTP_OK,
                                    kSaveInstrumentValidRequest,
                                    kSaveWithInvalidRequiredActionsResponse,
                                    4U);
}

TEST_F(WalletClientTest, SaveInstrumentFailedMalformedResponse) {
  EXPECT_CALL(delegate_,
              OnWalletError(WalletClient::MALFORMED_RESPONSE)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET, 1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_MALFORMED_RESPONSE);
  delegate_.ExpectLogWalletMalformedResponse(AutofillMetrics::SAVE_TO_WALLET);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  wallet_client_->SaveToWallet(instrument.Pass(),
                               scoped_ptr<Address>(),
                               NULL,
                               NULL);

  VerifyAndFinishFormEncodedRequest(net::HTTP_OK,
                                    kSaveInstrumentValidRequest,
                                    kSaveInvalidResponse,
                                    4U);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressSucceeded) {
  EXPECT_CALL(delegate_,
              OnDidSaveToWallet("saved_instrument_id",
                                "saved_address_id",
                                std::vector<RequiredAction>(),
                                std::vector<FormFieldError>())).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::SAVE_TO_WALLET,
      1);
  delegate_.ExpectBaselineMetrics();

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveToWallet(instrument.Pass(), address.Pass(), NULL, NULL);

  VerifyAndFinishFormEncodedRequest(net::HTTP_OK,
                                    kSaveInstrumentAndAddressValidRequest,
                                    kSaveInstrumentAndAddressValidResponse,
                                    4U);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressWithRequiredActionsSucceeded) {
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::SAVE_TO_WALLET,
      1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletRequiredActionMetric(
      AutofillMetrics::REQUIRE_PHONE_NUMBER);
  delegate_.ExpectWalletRequiredActionMetric(
      AutofillMetrics::INVALID_FORM_FIELD);

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(REQUIRE_PHONE_NUMBER);
  required_actions.push_back(INVALID_FORM_FIELD);

  std::vector<FormFieldError> form_errors;
  form_errors.push_back(FormFieldError(FormFieldError::INVALID_POSTAL_CODE,
                                       FormFieldError::SHIPPING_ADDRESS));

  EXPECT_CALL(delegate_,
              OnDidSaveToWallet(std::string(),
                                std::string(),
                                required_actions,
                                form_errors)).Times(1);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveToWallet(instrument.Pass(), address.Pass(), NULL, NULL);

  VerifyAndFinishFormEncodedRequest(
      net::HTTP_OK,
      kSaveInstrumentAndAddressValidRequest,
      kSaveInstrumentAndAddressWithRequiredActionsValidResponse,
      4U);
}

TEST_F(WalletClientTest, SaveInstrumentAndAddressFailedInvalidRequiredAction) {
  EXPECT_CALL(delegate_,
              OnWalletError(WalletClient::MALFORMED_RESPONSE)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(
      AutofillMetrics::SAVE_TO_WALLET,
      1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_MALFORMED_RESPONSE);
  delegate_.ExpectLogWalletMalformedResponse(AutofillMetrics::SAVE_TO_WALLET);

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  scoped_ptr<Address> address = GetTestSaveableAddress();
  wallet_client_->SaveToWallet(instrument.Pass(), address.Pass(), NULL, NULL);

  VerifyAndFinishFormEncodedRequest(net::HTTP_OK,
                                    kSaveInstrumentAndAddressValidRequest,
                                    kSaveWithInvalidRequiredActionsResponse,
                                    4U);
}

TEST_F(WalletClientTest, UpdateAddressSucceeded) {
  EXPECT_CALL(delegate_,
              OnDidSaveToWallet(std::string(),
                                "shipping_address_id",
                                std::vector<RequiredAction>(),
                                std::vector<FormFieldError>())).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET, 1);
  delegate_.ExpectBaselineMetrics();

  scoped_ptr<Address> reference_address = GetTestNonDefaultShippingAddress();
  wallet_client_->SaveToWallet(scoped_ptr<Instrument>(),
                               GetTestShippingAddress(),
                               NULL,
                               reference_address.get());

  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateAddressValidRequest,
                         kUpdateAddressValidResponse);
}

TEST_F(WalletClientTest, UpdateAddressWithRequiredActionsSucceeded) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET, 1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletRequiredActionMetric(
      AutofillMetrics::REQUIRE_PHONE_NUMBER);
  delegate_.ExpectWalletRequiredActionMetric(
      AutofillMetrics::INVALID_FORM_FIELD);

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(REQUIRE_PHONE_NUMBER);
  required_actions.push_back(INVALID_FORM_FIELD);

  std::vector<FormFieldError> form_errors;
  form_errors.push_back(FormFieldError(FormFieldError::INVALID_POSTAL_CODE,
                                       FormFieldError::SHIPPING_ADDRESS));

  EXPECT_CALL(delegate_, OnDidSaveToWallet(std::string(),
                                           std::string(),
                                           required_actions,
                                           form_errors)).Times(1);

  scoped_ptr<Address> reference_address = GetTestNonDefaultShippingAddress();
  wallet_client_->SaveToWallet(scoped_ptr<Instrument>(),
                               GetTestShippingAddress(),
                               NULL,
                               reference_address.get());

  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateAddressValidRequest,
                         kUpdateWithRequiredActionsValidResponse);
}

TEST_F(WalletClientTest, UpdateAddressFailedInvalidRequiredAction) {
  EXPECT_CALL(delegate_,
              OnWalletError(WalletClient::MALFORMED_RESPONSE)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET, 1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_MALFORMED_RESPONSE);
  delegate_.ExpectLogWalletMalformedResponse(AutofillMetrics::SAVE_TO_WALLET);

  scoped_ptr<Address> reference_address = GetTestNonDefaultShippingAddress();
  wallet_client_->SaveToWallet(scoped_ptr<Instrument>(),
                               GetTestShippingAddress(),
                               NULL,
                               reference_address.get());

  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateAddressValidRequest,
                         kSaveWithInvalidRequiredActionsResponse);
}

TEST_F(WalletClientTest, UpdateAddressMalformedResponse) {
  EXPECT_CALL(delegate_,
              OnWalletError(WalletClient::MALFORMED_RESPONSE)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET, 1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_MALFORMED_RESPONSE);
  delegate_.ExpectLogWalletMalformedResponse(AutofillMetrics::SAVE_TO_WALLET);

  scoped_ptr<Address> reference_address = GetTestNonDefaultShippingAddress();
  wallet_client_->SaveToWallet(scoped_ptr<Instrument>(),
                               GetTestShippingAddress(),
                               NULL,
                               reference_address.get());

  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateAddressValidRequest,
                         kUpdateMalformedResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentAddressSucceeded) {
  EXPECT_CALL(delegate_,
              OnDidSaveToWallet("instrument_id",
                                std::string(),
                                std::vector<RequiredAction>(),
                                std::vector<FormFieldError>())).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET,
                                           1);
  delegate_.ExpectBaselineMetrics();

  scoped_ptr<WalletItems::MaskedInstrument> reference_instrument =
      GetTestMaskedInstrument();
  wallet_client_->SaveToWallet(GetTestAddressUpgradeInstrument(),
                               scoped_ptr<Address>(),
                               reference_instrument.get(),
                               NULL);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateInstrumentAddressValidRequest,
                         kUpdateInstrumentValidResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentExpirationDateSuceeded) {
  EXPECT_CALL(delegate_,
              OnDidSaveToWallet("instrument_id",
                                std::string(),
                                std::vector<RequiredAction>(),
                                std::vector<FormFieldError>())).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET,
                                           1);
  delegate_.ExpectBaselineMetrics();

  scoped_ptr<Instrument> instrument = GetTestExpirationDateChangeInstrument();
  scoped_ptr<WalletItems::MaskedInstrument> reference_instrument =
      GetTestMaskedInstrumentWithId("instrument_id");

  int new_month = instrument->expiration_month();
  int new_year = instrument->expiration_year();
  ASSERT_TRUE(new_month != reference_instrument->expiration_month() ||
              new_year != reference_instrument->expiration_year());

  wallet_client_->SaveToWallet(instrument.Pass(),
                               scoped_ptr<Address>(),
                               reference_instrument.get(),
                               NULL);

  VerifyAndFinishFormEncodedRequest(net::HTTP_OK,
                                    kUpdateInstrumentExpirationDateValidRequest,
                                    kUpdateInstrumentValidResponse,
                                    3U);
}

TEST_F(WalletClientTest, UpdateInstrumentAddressWithNameChangeSucceeded) {
  EXPECT_CALL(delegate_,
              OnDidSaveToWallet("instrument_id",
                                std::string(),
                                std::vector<RequiredAction>(),
                                std::vector<FormFieldError>())).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET,
                                           1);
  delegate_.ExpectBaselineMetrics();

  scoped_ptr<WalletItems::MaskedInstrument> reference_instrument =
      GetTestMaskedInstrument();
  wallet_client_->SaveToWallet(GetTestAddressNameChangeInstrument(),
                               scoped_ptr<Address>(),
                               reference_instrument.get(),
                               NULL);

  VerifyAndFinishFormEncodedRequest(
      net::HTTP_OK,
      kUpdateInstrumentAddressWithNameChangeValidRequest,
      kUpdateInstrumentValidResponse,
      3U);
}

TEST_F(WalletClientTest, UpdateInstrumentWithRequiredActionsSucceeded) {
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET,
                                           1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletRequiredActionMetric(
      AutofillMetrics::REQUIRE_PHONE_NUMBER);
  delegate_.ExpectWalletRequiredActionMetric(
      AutofillMetrics::INVALID_FORM_FIELD);

  std::vector<RequiredAction> required_actions;
  required_actions.push_back(REQUIRE_PHONE_NUMBER);
  required_actions.push_back(INVALID_FORM_FIELD);

  std::vector<FormFieldError> form_errors;
  form_errors.push_back(FormFieldError(FormFieldError::INVALID_POSTAL_CODE,
                                       FormFieldError::SHIPPING_ADDRESS));

  EXPECT_CALL(delegate_,
              OnDidSaveToWallet(std::string(),
                                std::string(),
                                required_actions,
                                form_errors)).Times(1);

  scoped_ptr<WalletItems::MaskedInstrument> reference_instrument =
      GetTestMaskedInstrument();
  wallet_client_->SaveToWallet(GetTestAddressUpgradeInstrument(),
                               scoped_ptr<Address>(),
                               reference_instrument.get(),
                               NULL);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateInstrumentAddressValidRequest,
                         kUpdateWithRequiredActionsValidResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentFailedInvalidRequiredAction) {
  EXPECT_CALL(delegate_,
              OnWalletError(WalletClient::MALFORMED_RESPONSE)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET,
                                           1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_MALFORMED_RESPONSE);
  delegate_.ExpectLogWalletMalformedResponse(AutofillMetrics::SAVE_TO_WALLET);

  scoped_ptr<WalletItems::MaskedInstrument> reference_instrument =
      GetTestMaskedInstrument();
  wallet_client_->SaveToWallet(GetTestAddressUpgradeInstrument(),
                               scoped_ptr<Address>(),
                               reference_instrument.get(),
                               NULL);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateInstrumentAddressValidRequest,
                         kSaveWithInvalidRequiredActionsResponse);
}

TEST_F(WalletClientTest, UpdateInstrumentMalformedResponse) {
  EXPECT_CALL(delegate_,
              OnWalletError(WalletClient::MALFORMED_RESPONSE)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::SAVE_TO_WALLET,
                                           1);
  delegate_.ExpectBaselineMetrics();
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_MALFORMED_RESPONSE);
  delegate_.ExpectLogWalletMalformedResponse(AutofillMetrics::SAVE_TO_WALLET);

  scoped_ptr<WalletItems::MaskedInstrument> reference_instrument =
      GetTestMaskedInstrument();
  wallet_client_->SaveToWallet(GetTestAddressUpgradeInstrument(),
                               scoped_ptr<Address>(),
                               reference_instrument.get(),
                               NULL);

  VerifyAndFinishRequest(net::HTTP_OK,
                         kUpdateInstrumentAddressValidRequest,
                         kUpdateMalformedResponse);
}

TEST_F(WalletClientTest, HasRequestInProgress) {
  EXPECT_FALSE(wallet_client_->HasRequestInProgress());
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           1);
  delegate_.ExpectBaselineMetrics();

  wallet_client_->GetWalletItems(base::string16(), base::string16());
  EXPECT_TRUE(wallet_client_->HasRequestInProgress());

  VerifyAndFinishRequest(net::HTTP_OK,
                         kGetWalletItemsValidRequest,
                         kGetWalletItemsValidResponse);
  EXPECT_FALSE(wallet_client_->HasRequestInProgress());
}

// 500 (INTERNAL_SERVER_ERROR) - response json is parsed.
TEST_F(WalletClientTest, ErrorResponse500) {
  EXPECT_FALSE(wallet_client_->HasRequestInProgress());
  delegate_.ExpectBaselineMetrics();
  wallet_client_->GetWalletItems(base::string16(), base::string16());
  EXPECT_TRUE(wallet_client_->HasRequestInProgress());
  testing::Mock::VerifyAndClear(delegate_.metric_logger());

  EXPECT_CALL(delegate_, OnWalletError(
      WalletClient::SERVICE_UNAVAILABLE)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           1);
  delegate_.ExpectWalletErrorMetric(
      AutofillMetrics::WALLET_SERVICE_UNAVAILABLE);

  VerifyAndFinishRequest(net::HTTP_INTERNAL_SERVER_ERROR,
                         kGetWalletItemsValidRequest,
                         kErrorResponse);
}

// 403 (FORBIDDEN) - response json is parsed.
TEST_F(WalletClientTest, ErrorResponse403) {
  EXPECT_FALSE(wallet_client_->HasRequestInProgress());
  delegate_.ExpectBaselineMetrics();
  wallet_client_->GetWalletItems(base::string16(), base::string16());
  EXPECT_TRUE(wallet_client_->HasRequestInProgress());
  testing::Mock::VerifyAndClear(delegate_.metric_logger());

  EXPECT_CALL(delegate_, OnWalletError(WalletClient::SPENDING_LIMIT_EXCEEDED))
      .Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           1);
  delegate_.ExpectWalletErrorMetric(
      AutofillMetrics::WALLET_SPENDING_LIMIT_EXCEEDED);

  VerifyAndFinishRequest(net::HTTP_FORBIDDEN,
                         kGetWalletItemsValidRequest,
                         kErrorResponseSpendingLimitExceeded);
}

// 400 (BAD_REQUEST) - response json is ignored.
TEST_F(WalletClientTest, ErrorResponse400) {
  EXPECT_FALSE(wallet_client_->HasRequestInProgress());
  delegate_.ExpectBaselineMetrics();
  wallet_client_->GetWalletItems(base::string16(), base::string16());
  EXPECT_TRUE(wallet_client_->HasRequestInProgress());
  testing::Mock::VerifyAndClear(delegate_.metric_logger());

  EXPECT_CALL(delegate_, OnWalletError(WalletClient::BAD_REQUEST)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           1);
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_BAD_REQUEST);

  VerifyAndFinishRequest(
      net::HTTP_BAD_REQUEST, kGetWalletItemsValidRequest, kErrorResponse);
}

// Anything else - response json is ignored.
TEST_F(WalletClientTest, ErrorResponseOther) {
  EXPECT_FALSE(wallet_client_->HasRequestInProgress());
  delegate_.ExpectBaselineMetrics();
  wallet_client_->GetWalletItems(base::string16(), base::string16());
  EXPECT_TRUE(wallet_client_->HasRequestInProgress());
  testing::Mock::VerifyAndClear(delegate_.metric_logger());

  EXPECT_CALL(delegate_, OnWalletError(WalletClient::NETWORK_ERROR)).Times(1);
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           1);
  delegate_.ExpectWalletErrorMetric(AutofillMetrics::WALLET_NETWORK_ERROR);

  VerifyAndFinishRequest(
      net::HTTP_NOT_FOUND, kGetWalletItemsValidRequest, kErrorResponse);
}

TEST_F(WalletClientTest, CancelRequest) {
  EXPECT_FALSE(wallet_client_->HasRequestInProgress());
  delegate_.ExpectLogWalletApiCallDuration(AutofillMetrics::GET_WALLET_ITEMS,
                                           0);
  delegate_.ExpectBaselineMetrics();

  wallet_client_->GetWalletItems(base::string16(), base::string16());
  EXPECT_TRUE(wallet_client_->HasRequestInProgress());
  wallet_client_->CancelRequest();
  EXPECT_FALSE(wallet_client_->HasRequestInProgress());
}

}  // namespace wallet
}  // namespace autofill
