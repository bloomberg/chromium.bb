// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_util.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/account_service_flag_fetcher.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kAccountId[] = "user@gmail.com";
const char kDifferentAccountId[] = "some_other_user@gmail.com";

const int kGaiaAuthFetcherURLFetcherID = 0;

// TODO(treib): This class should really live in components/signin/ next to the
// AccountServiceFlagFetcher, but it uses the FakePO2TS which lives in
// chrome/browser/ (because it uses the AndroidPO2TS which depends on stuff from
// chrome/browser/). So when the AndroidPO2TS is componentized, then this should
// move as well.
class AccountServiceFlagFetcherTest : public testing::Test {
 public:
  AccountServiceFlagFetcherTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::MessageLoopProxy::current())) {
    service_flags_.push_back("some_flag");
    service_flags_.push_back("another_flag");
    service_flags_.push_back("andonemore");
  }

  MOCK_METHOD2(OnFlagsFetched,
               void(AccountServiceFlagFetcher::ResultCode result,
                    const std::vector<std::string>& flags));

 protected:
  net::TestURLFetcher* GetLoginURLFetcher() {
    net::TestURLFetcher* fetcher =
        url_fetcher_factory_.GetFetcherByID(kGaiaAuthFetcherURLFetcherID);
    EXPECT_TRUE(fetcher);

    EXPECT_EQ(GaiaUrls::GetInstance()->oauth1_login_url(),
              fetcher->GetOriginalURL());

    return fetcher;
  }

  net::TestURLFetcher* GetGetUserInfoURLFetcher() {
    net::TestURLFetcher* fetcher =
        url_fetcher_factory_.GetFetcherByID(kGaiaAuthFetcherURLFetcherID);
    EXPECT_TRUE(fetcher);

    EXPECT_EQ(GaiaUrls::GetInstance()->get_user_info_url(),
              fetcher->GetOriginalURL());

    return fetcher;
  }

  void SendValidLoginResponse() {
    net::TestURLFetcher* fetcher = GetLoginURLFetcher();
    fetcher->set_status(
        net::URLRequestStatus(net::URLRequestStatus::SUCCESS, 0));
    fetcher->set_response_code(net::HTTP_OK);
    fetcher->SetResponseString(std::string("SID=sid\nLSID=lsid\nAuth=auth\n"));
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void SendFailedLoginResponse() {
    net::TestURLFetcher* fetcher = GetLoginURLFetcher();
    fetcher->set_status(
        net::URLRequestStatus(net::URLRequestStatus::CANCELED, 0));
    fetcher->set_response_code(net::HTTP_OK);
    fetcher->SetResponseString(std::string());
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void SendValidGetUserInfoResponse() {
    net::TestURLFetcher* fetcher = GetGetUserInfoURLFetcher();
    fetcher->set_status(
        net::URLRequestStatus(net::URLRequestStatus::SUCCESS, 0));
    fetcher->set_response_code(net::HTTP_OK);
    fetcher->SetResponseString(BuildGetUserInfoResponse());
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void SendInvalidGetUserInfoResponse() {
    net::TestURLFetcher* fetcher = GetGetUserInfoURLFetcher();
    fetcher->set_status(
        net::URLRequestStatus(net::URLRequestStatus::SUCCESS, 0));
    fetcher->set_response_code(net::HTTP_OK);
    fetcher->SetResponseString(std::string("allServicesIsMissing=true"));
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void SendFailedGetUserInfoResponse() {
    net::TestURLFetcher* fetcher = GetGetUserInfoURLFetcher();
    fetcher->set_status(
        net::URLRequestStatus(net::URLRequestStatus::CANCELED, 0));
    fetcher->set_response_code(net::HTTP_OK);
    fetcher->SetResponseString(std::string());
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  std::string BuildGetUserInfoResponse() const {
    return "allServices=" + JoinString(service_flags_, ',');
  }

  base::MessageLoop message_loop_;
  FakeProfileOAuth2TokenService token_service_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  net::ResponseCookies cookies_;
  std::vector<std::string> service_flags_;
};

TEST_F(AccountServiceFlagFetcherTest, Success) {
  token_service_.UpdateCredentials(kAccountId, "refresh_token");

  AccountServiceFlagFetcher fetcher(
      kAccountId,
      &token_service_,
      request_context_.get(),
      base::Bind(&AccountServiceFlagFetcherTest::OnFlagsFetched,
                 base::Unretained(this)));

  // Since a refresh token is already available, we should immediately get a
  // request for an access token.
  EXPECT_EQ(1U, token_service_.GetPendingRequests().size());

  token_service_.IssueAllTokensForAccount(
      kAccountId,
      "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  SendValidLoginResponse();

  EXPECT_CALL(*this, OnFlagsFetched(AccountServiceFlagFetcher::SUCCESS,
                                    service_flags_));
  SendValidGetUserInfoResponse();
}

TEST_F(AccountServiceFlagFetcherTest, SuccessAfterWaitingForRefreshToken) {
  AccountServiceFlagFetcher fetcher(
      kAccountId,
      &token_service_,
      request_context_.get(),
      base::Bind(&AccountServiceFlagFetcherTest::OnFlagsFetched,
                 base::Unretained(this)));

  // Since there is no refresh token yet, we should not get a request for an
  // access token at this point.
  EXPECT_EQ(0U, token_service_.GetPendingRequests().size());

  token_service_.UpdateCredentials(kAccountId, "refresh_token");

  // Now there is a refresh token and we should have got a request for an
  // access token.
  EXPECT_EQ(1U, token_service_.GetPendingRequests().size());

  token_service_.IssueAllTokensForAccount(
      kAccountId,
      "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  SendValidLoginResponse();

  EXPECT_CALL(*this, OnFlagsFetched(AccountServiceFlagFetcher::SUCCESS,
                                    service_flags_));
  SendValidGetUserInfoResponse();
}

TEST_F(AccountServiceFlagFetcherTest, NoRefreshToken) {
  AccountServiceFlagFetcher fetcher(
      kAccountId,
      &token_service_,
      request_context_.get(),
      base::Bind(&AccountServiceFlagFetcherTest::OnFlagsFetched,
                 base::Unretained(this)));

  token_service_.UpdateCredentials(kDifferentAccountId, "refresh_token");

  // Credentials for a different user should be ignored, i.e. not result in a
  // request for an access token.
  EXPECT_EQ(0U, token_service_.GetPendingRequests().size());

  // After all refresh tokens have been loaded, there is still no token for our
  // user, so we expect a token error.
  EXPECT_CALL(*this, OnFlagsFetched(AccountServiceFlagFetcher::TOKEN_ERROR,
                                    std::vector<std::string>()));
  token_service_.IssueAllRefreshTokensLoaded();
}

TEST_F(AccountServiceFlagFetcherTest, GetTokenFailure) {
  token_service_.UpdateCredentials(kAccountId, "refresh_token");

  AccountServiceFlagFetcher fetcher(
      kAccountId,
      &token_service_,
      request_context_.get(),
      base::Bind(&AccountServiceFlagFetcherTest::OnFlagsFetched,
                 base::Unretained(this)));

  // On failure to get an access token we expect a token error.
  EXPECT_CALL(*this, OnFlagsFetched(AccountServiceFlagFetcher::TOKEN_ERROR,
                                    std::vector<std::string>()));
  token_service_.IssueErrorForAllPendingRequestsForAccount(
      kAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
}

TEST_F(AccountServiceFlagFetcherTest, ClientLoginFailure) {
  token_service_.UpdateCredentials(kAccountId, "refresh_token");

  AccountServiceFlagFetcher fetcher(
      kAccountId,
      &token_service_,
      request_context_.get(),
      base::Bind(&AccountServiceFlagFetcherTest::OnFlagsFetched,
                 base::Unretained(this)));

  token_service_.IssueAllTokensForAccount(
      kAccountId,
      "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  // Login failure should result in a service error.
  EXPECT_CALL(*this, OnFlagsFetched(AccountServiceFlagFetcher::SERVICE_ERROR,
                                    std::vector<std::string>()));
  SendFailedLoginResponse();
}

TEST_F(AccountServiceFlagFetcherTest, GetUserInfoInvalidResponse) {
  token_service_.UpdateCredentials(kAccountId, "refresh_token");

  AccountServiceFlagFetcher fetcher(
      kAccountId,
      &token_service_,
      request_context_.get(),
      base::Bind(&AccountServiceFlagFetcherTest::OnFlagsFetched,
                 base::Unretained(this)));

  token_service_.IssueAllTokensForAccount(
      kAccountId,
      "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  SendValidLoginResponse();

  // Invalid response data from GetUserInfo should result in a service error.
  EXPECT_CALL(*this, OnFlagsFetched(AccountServiceFlagFetcher::SERVICE_ERROR,
                                    std::vector<std::string>()));
  SendInvalidGetUserInfoResponse();
}

TEST_F(AccountServiceFlagFetcherTest, GetUserInfoFailure) {
  token_service_.UpdateCredentials(kAccountId, "refresh_token");

  AccountServiceFlagFetcher fetcher(
      kAccountId,
      &token_service_,
      request_context_.get(),
      base::Bind(&AccountServiceFlagFetcherTest::OnFlagsFetched,
                 base::Unretained(this)));

  token_service_.IssueAllTokensForAccount(
      kAccountId,
      "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  SendValidLoginResponse();

  // Failed GetUserInfo call should result in a service error.
  EXPECT_CALL(*this, OnFlagsFetched(AccountServiceFlagFetcher::SERVICE_ERROR,
                                    std::vector<std::string>()));
  SendFailedGetUserInfoResponse();
}

TEST_F(AccountServiceFlagFetcherTest, DestroyFetcher) {
  token_service_.UpdateCredentials(kAccountId, "refresh_token");

  // When the fetcher is destroyed before the request completes, OnFlagsFetched
  // should not be called.
  EXPECT_CALL(*this, OnFlagsFetched(testing::_, testing::_)).Times(0);

  AccountServiceFlagFetcher fetcher(
      kAccountId,
      &token_service_,
      request_context_.get(),
      base::Bind(&AccountServiceFlagFetcherTest::OnFlagsFetched,
                 base::Unretained(this)));

  token_service_.IssueAllTokensForAccount(
      kAccountId,
      "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  SendValidLoginResponse();
  // Do not send a GetUserInfo response, but make sure the request is there.
  GetGetUserInfoURLFetcher();
}
