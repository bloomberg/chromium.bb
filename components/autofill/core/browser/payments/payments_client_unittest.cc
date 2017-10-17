// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "google_apis/gaia/fake_identity_provider.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace payments {

class PaymentsClientTest : public testing::Test,
                           public PaymentsClientUnmaskDelegate,
                           public PaymentsClientSaveDelegate {
 public:
  PaymentsClientTest() : result_(AutofillClient::NONE) {}
  ~PaymentsClientTest() override {}

  void SetUp() override {
    // Silence the warning for mismatching sync and Payments servers.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWalletServiceUseSandbox, "0");

    result_ = AutofillClient::NONE;
    server_id_.clear();
    real_pan_.clear();
    legal_message_.reset();

    request_context_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());
    token_service_.reset(new FakeOAuth2TokenService());
    identity_provider_.reset(new FakeIdentityProvider(token_service_.get()));
    TestingPrefServiceSimple pref_service_;
    client_.reset(new PaymentsClient(request_context_.get(), &pref_service_,
                                     identity_provider_.get(), this, this));
  }

  void TearDown() override { client_.reset(); }

  void DisableAutofillSendBillingCustomerNumberExperiment() {
    scoped_feature_list_.InitAndDisableFeature(
        kAutofillSendBillingCustomerNumber);
  }

  // PaymentsClientUnmaskDelegate:
  void OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                       const std::string& real_pan) override {
    result_ = result;
    real_pan_ = real_pan;
  }

  // PaymentsClientSaveDelegate:
  void OnDidGetUploadDetails(
      AutofillClient::PaymentsRpcResult result,
      const base::string16& context_token,
      std::unique_ptr<base::DictionaryValue> legal_message) override {
    result_ = result;
    legal_message_ = std::move(legal_message);
  }
  void OnDidUploadCard(AutofillClient::PaymentsRpcResult result,
                       const std::string& server_id) override {
    result_ = result;
    server_id_ = server_id;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  void StartUnmasking() {
    token_service_->AddAccount("example@gmail.com");
    identity_provider_->LogIn("example@gmail.com");
    PaymentsClient::UnmaskRequestDetails request_details;
    request_details.billing_customer_number = 111222333444;
    request_details.card = test::GetMaskedServerCard();
    request_details.user_response.cvc = base::ASCIIToUTF16("123");
    request_details.risk_data = "some risk data";
    client_->UnmaskCard(request_details);
  }

  void StartGettingUploadDetails() {
    token_service_->AddAccount("example@gmail.com");
    identity_provider_->LogIn("example@gmail.com");
    client_->GetUploadDetails(BuildTestProfiles(), std::vector<const char*>(),
                              "language-LOCALE");
  }

  void StartUploading() {
    token_service_->AddAccount("example@gmail.com");
    identity_provider_->LogIn("example@gmail.com");
    PaymentsClient::UploadRequestDetails request_details;
    request_details.billing_customer_number = 111222333444;
    request_details.card = test::GetCreditCard();
    request_details.cvc = base::ASCIIToUTF16("123");
    request_details.context_token = base::ASCIIToUTF16("context token");
    request_details.risk_data = "some risk data";
    request_details.app_locale = "language-LOCALE";
    request_details.profiles = BuildTestProfiles();
    client_->UploadCard(request_details);
  }

  const std::string& GetUploadData() {
    return factory_.GetFetcherByID(0)->upload_data();
  }

  void IssueOAuthToken() {
    token_service_->IssueAllTokensForAccount(
        "example@gmail.com", "totally_real_token",
        base::Time::Now() + base::TimeDelta::FromDays(10));

    // Verify the auth header.
    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    net::HttpRequestHeaders request_headers;
    fetcher->GetExtraRequestHeaders(&request_headers);
    std::string auth_header_value;
    EXPECT_TRUE(request_headers.GetHeader(
        net::HttpRequestHeaders::kAuthorization, &auth_header_value))
        << request_headers.ToString();
    EXPECT_EQ("Bearer totally_real_token", auth_header_value);
  }

  void ReturnResponse(net::HttpStatusCode response_code,
                      const std::string& response_body) {
    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response_body);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  AutofillClient::PaymentsRpcResult result_;
  std::string server_id_;
  std::string real_pan_;
  std::unique_ptr<base::DictionaryValue> legal_message_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  net::TestURLFetcherFactory factory_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  std::unique_ptr<FakeOAuth2TokenService> token_service_;
  std::unique_ptr<FakeIdentityProvider> identity_provider_;
  std::unique_ptr<PaymentsClient> client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentsClientTest);

  std::vector<AutofillProfile> BuildTestProfiles() {
    std::vector<AutofillProfile> profiles;
    profiles.push_back(BuildProfile("John", "Smith", "1234 Main St.", "Miami",
                                    "FL", "32006", "212-555-0162"));
    profiles.push_back(BuildProfile("Pat", "Jones", "432 Oak Lane", "Lincoln",
                                    "OH", "43005", "(834)555-0090"));
    return profiles;
  }

  AutofillProfile BuildProfile(base::StringPiece first_name,
                               base::StringPiece last_name,
                               base::StringPiece address_line,
                               base::StringPiece city,
                               base::StringPiece state,
                               base::StringPiece zip,
                               base::StringPiece phone_number) {
    AutofillProfile profile;

    profile.SetInfo(NAME_FIRST, ASCIIToUTF16(first_name), "en-US");
    profile.SetInfo(NAME_LAST, ASCIIToUTF16(last_name), "en-US");
    profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16(address_line), "en-US");
    profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16(city), "en-US");
    profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16(state), "en-US");
    profile.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16(zip), "en-US");
    profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16(phone_number),
                    "en-US");

    return profile;
  }
};

TEST_F(PaymentsClientTest, OAuthError) {
  StartUnmasking();
  token_service_->IssueErrorForAllPendingRequestsForAccount(
      "example@gmail.com",
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_TRUE(real_pan_.empty());
}

TEST_F(PaymentsClientTest,
       UnmaskRequestIncludesBillingCustomerNumberInRequestIfExperimentOn) {
  StartUnmasking();

  // Verify that the billing customer number is included in the request.
  EXPECT_TRUE(
      GetUploadData().find("%22external_customer_id%22:%22111222333444%22") !=
      std::string::npos);
}

TEST_F(
    PaymentsClientTest,
    UnmaskRequestDoesNotIncludeBillingCustomerNumberInRequestIfExperimentOff) {
  DisableAutofillSendBillingCustomerNumberExperiment();

  StartUnmasking();

  // Verify that the billing customer number is not included in the request.
  EXPECT_TRUE(GetUploadData().find("external_customer_id") ==
              std::string::npos);
}

TEST_F(PaymentsClientTest, UnmaskSuccess) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"pan\": \"1234\" }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_EQ("1234", real_pan_);
}

TEST_F(PaymentsClientTest, GetDetailsSuccess) {
  StartGettingUploadDetails();
  ReturnResponse(
      net::HTTP_OK,
      "{ \"context_token\": \"some_token\", \"legal_message\": {} }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_NE(nullptr, legal_message_.get());
}

TEST_F(PaymentsClientTest, GetDetailsRemovesNonLocationData) {
  StartGettingUploadDetails();

  // Verify that the recipient name field and test names appear nowhere in the
  // upload data.
  EXPECT_TRUE(GetUploadData().find(PaymentsClient::kRecipientName) ==
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("John") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Smith") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Pat") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Jones") == std::string::npos);

  // Verify that the phone number field and test numbers appear nowhere in the
  // upload data.
  EXPECT_TRUE(GetUploadData().find(PaymentsClient::kPhoneNumber) ==
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("212") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("555") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("0162") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("834") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("0090") == std::string::npos);
}

TEST_F(PaymentsClientTest, UploadSuccessWithoutServerId) {
  StartUploading();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_TRUE(server_id_.empty());
}

TEST_F(PaymentsClientTest, UploadSuccessWithServerId) {
  StartUploading();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"credit_card_id\": \"InstrumentData:1\" }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_EQ("InstrumentData:1", server_id_);
}

TEST_F(PaymentsClientTest, UploadIncludesNonLocationData) {
  StartUploading();

  // Verify that the recipient name field and test names do appear in the upload
  // data.
  EXPECT_TRUE(GetUploadData().find(PaymentsClient::kRecipientName) !=
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("John") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Smith") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Pat") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Jones") != std::string::npos);

  // Verify that the phone number field and test numbers do appear in the upload
  // data.
  EXPECT_TRUE(GetUploadData().find(PaymentsClient::kPhoneNumber) !=
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("212") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("555") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("0162") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("834") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("0090") != std::string::npos);
}

TEST_F(PaymentsClientTest,
       UploadRequestIncludesBillingCustomerNumberInRequestIfExperimentOn) {
  StartUploading();

  // Verify that the billing customer number is included in the request.
  EXPECT_TRUE(
      GetUploadData().find("%22external_customer_id%22:%22111222333444%22") !=
      std::string::npos);
}

TEST_F(
    PaymentsClientTest,
    UploadRequestDoesNotIncludeBillingCustomerNumberInRequestIfExperimentOff) {
  DisableAutofillSendBillingCustomerNumberExperiment();

  StartUploading();

  // Verify that the billing customer number is not included in the request.
  EXPECT_TRUE(GetUploadData().find("external_customer_id") ==
              std::string::npos);
}

TEST_F(PaymentsClientTest, GetDetailsFollowedByUploadSuccess) {
  StartGettingUploadDetails();
  ReturnResponse(
      net::HTTP_OK,
      "{ \"context_token\": \"some_token\", \"legal_message\": {} }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);

  result_ = AutofillClient::NONE;

  StartUploading();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
}

TEST_F(PaymentsClientTest, UnmaskMissingPan) {
  StartUnmasking();
  ReturnResponse(net::HTTP_OK, "{}");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
}

TEST_F(PaymentsClientTest, GetDetailsMissingContextToken) {
  StartGettingUploadDetails();
  ReturnResponse(net::HTTP_OK, "{ \"legal_message\": {} }");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
}

TEST_F(PaymentsClientTest, GetDetailsMissingLegalMessage) {
  StartGettingUploadDetails();
  ReturnResponse(net::HTTP_OK, "{ \"context_token\": \"some_token\" }");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ(nullptr, legal_message_.get());
}

TEST_F(PaymentsClientTest, RetryFailure) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"error\": { \"code\": \"INTERNAL\" } }");
  EXPECT_EQ(AutofillClient::TRY_AGAIN_FAILURE, result_);
  EXPECT_EQ("", real_pan_);
}

TEST_F(PaymentsClientTest, PermanentFailure) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"error\": { \"code\": \"ANYTHING_ELSE\" } }");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ("", real_pan_);
}

TEST_F(PaymentsClientTest, MalformedResponse) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"error_code\": \"WRONG_JSON_FORMAT\" }");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ("", real_pan_);
}

TEST_F(PaymentsClientTest, ReauthNeeded) {
  {
    StartUnmasking();
    IssueOAuthToken();
    ReturnResponse(net::HTTP_UNAUTHORIZED, "");
    // No response yet.
    EXPECT_EQ(AutofillClient::NONE, result_);
    EXPECT_EQ("", real_pan_);

    // Second HTTP_UNAUTHORIZED causes permanent failure.
    IssueOAuthToken();
    ReturnResponse(net::HTTP_UNAUTHORIZED, "");
    EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
    EXPECT_EQ("", real_pan_);
  }

  result_ = AutofillClient::NONE;
  real_pan_.clear();

  {
    StartUnmasking();
    IssueOAuthToken();
    ReturnResponse(net::HTTP_UNAUTHORIZED, "");
    // No response yet.
    EXPECT_EQ(AutofillClient::NONE, result_);
    EXPECT_EQ("", real_pan_);

    // HTTP_OK after first HTTP_UNAUTHORIZED results in success.
    IssueOAuthToken();
    ReturnResponse(net::HTTP_OK, "{ \"pan\": \"1234\" }");
    EXPECT_EQ(AutofillClient::SUCCESS, result_);
    EXPECT_EQ("1234", real_pan_);
  }
}

TEST_F(PaymentsClientTest, NetworkError) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_REQUEST_TIMEOUT, std::string());
  EXPECT_EQ(AutofillClient::NETWORK_ERROR, result_);
  EXPECT_EQ("", real_pan_);
}

TEST_F(PaymentsClientTest, OtherError) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_FORBIDDEN, std::string());
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ("", real_pan_);
}

}  // namespace payments
}  // namespace autofill
