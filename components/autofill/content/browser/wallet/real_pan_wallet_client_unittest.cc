// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/thread_task_runner_handle.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/wallet/real_pan_wallet_client.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/fake_identity_provider.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace wallet {

class RealPanWalletClientTest : public testing::Test,
                                public RealPanWalletClient::Delegate {
 public:
  RealPanWalletClientTest() : result_(AutofillClient::SUCCESS) {}
  ~RealPanWalletClientTest() override {}

  void SetUp() override {
    // Silence the warning for mismatching sync and wallet servers.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWalletServiceUseSandbox, "0");

    request_context_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());
    token_service_.reset(new FakeOAuth2TokenService());
    identity_provider_.reset(new FakeIdentityProvider(token_service_.get()));
    client_.reset(new RealPanWalletClient(request_context_.get(), this));
  }

  void TearDown() override { client_.reset(); }

  // RealPanWalletClient::Delegate

  IdentityProvider* GetIdentityProvider() override {
    return identity_provider_.get();
  }

  void OnDidGetRealPan(AutofillClient::GetRealPanResult result,
                       const std::string& real_pan) override {
    result_ = result;
    real_pan_ = real_pan;
  }

 protected:
  void StartUnmasking() {
    token_service_->AddAccount("example@gmail.com");
    identity_provider_->LogIn("example@gmail.com");
    CreditCard card = test::GetMaskedServerCard();
    CardUnmaskDelegate::UnmaskResponse response;
    response.cvc = base::ASCIIToUTF16("123");
    client_->UnmaskCard(card, response);
  }

  void IssueOAuthToken() {
    token_service_->IssueAllTokensForAccount(
        "example@gmail.com",
        "totally_real_token",
        base::Time::Now() + base::TimeDelta::FromDays(10));

    // Verify the auth header.
    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    net::HttpRequestHeaders request_headers;
    fetcher->GetExtraRequestHeaders(&request_headers);
    std::string auth_header_value;
    EXPECT_TRUE(request_headers.GetHeader(
        net::HttpRequestHeaders::kAuthorization,
        &auth_header_value)) << request_headers.ToString();
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

  AutofillClient::GetRealPanResult result_;
  std::string real_pan_;

  content::TestBrowserThreadBundle thread_bundle_;
  net::TestURLFetcherFactory factory_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  scoped_ptr<FakeOAuth2TokenService> token_service_;
  scoped_ptr<FakeIdentityProvider> identity_provider_;
  scoped_ptr<RealPanWalletClient> client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RealPanWalletClientTest);
};

TEST_F(RealPanWalletClientTest, OAuthError) {
  StartUnmasking();
  token_service_->IssueErrorForAllPendingRequestsForAccount(
      "example@gmail.com",
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_TRUE(real_pan_.empty());
}

TEST_F(RealPanWalletClientTest, Success) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"pan\": \"1234\" }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_EQ("1234", real_pan_);
}

TEST_F(RealPanWalletClientTest, RetryFailure) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"error\": { \"code\": \"INTERNAL\" } }");
  EXPECT_EQ(AutofillClient::TRY_AGAIN_FAILURE, result_);
  EXPECT_EQ("", real_pan_);
}

TEST_F(RealPanWalletClientTest, PermanentFailure) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
      "{ \"error\": { \"code\": \"ANYTHING_ELSE\" } }");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ("", real_pan_);
}

TEST_F(RealPanWalletClientTest, MalformedResponse) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
      "{ \"error_code\": \"WRONG_JSON_FORMAT\" }");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ("", real_pan_);
}

TEST_F(RealPanWalletClientTest, ReauthNeeded) {
  {
    StartUnmasking();
    IssueOAuthToken();
    ReturnResponse(net::HTTP_UNAUTHORIZED, "");
    // No response yet.
    EXPECT_EQ(AutofillClient::SUCCESS, result_);
    EXPECT_EQ("", real_pan_);

    // Second HTTP_UNAUTHORIZED causes permanent failure.
    IssueOAuthToken();
    ReturnResponse(net::HTTP_UNAUTHORIZED, "");
    EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
    EXPECT_EQ("", real_pan_);
  }

  result_ = AutofillClient::SUCCESS;
  real_pan_.clear();

  {
    StartUnmasking();
    IssueOAuthToken();
    ReturnResponse(net::HTTP_UNAUTHORIZED, "");
    // No response yet.
    EXPECT_EQ(AutofillClient::SUCCESS, result_);
    EXPECT_EQ("", real_pan_);

    // HTTP_OK after first HTTP_UNAUTHORIZED results in success.
    IssueOAuthToken();
    ReturnResponse(net::HTTP_OK, "{ \"pan\": \"1234\" }");
    EXPECT_EQ(AutofillClient::SUCCESS, result_);
    EXPECT_EQ("1234", real_pan_);
  }
}

TEST_F(RealPanWalletClientTest, NetworkError) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_REQUEST_TIMEOUT, std::string());
  EXPECT_EQ(AutofillClient::NETWORK_ERROR, result_);
  EXPECT_EQ("", real_pan_);
}

TEST_F(RealPanWalletClientTest, OtherError) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_FORBIDDEN, std::string());
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ("", real_pan_);
}

}  // namespace autofill
}  // namespace wallet
