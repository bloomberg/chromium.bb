// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for GaiaAuthenticator2.
// Originally ported from GoogleAuthenticator tests.

#include <string>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_authenticator2.h"
#include "chrome/common/net/gaia/gaia_authenticator2_unittest.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/net/http_return.h"
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

class GaiaAuthenticator2Test : public testing::Test {
 public:
  GaiaAuthenticator2Test()
    : client_login_source_(GaiaAuthenticator2::kClientLoginUrl),
      issue_auth_token_source_(GaiaAuthenticator2::kIssueAuthTokenUrl) {}

  void RunParsingTest(const std::string& data,
                      const std::string& sid,
                      const std::string& lsid,
                      const std::string& token) {
    std::string out_sid;
    std::string out_lsid;
    std::string out_token;

    GaiaAuthenticator2::ParseClientLoginResponse(data,
                                                 &out_sid,
                                                 &out_lsid,
                                                 &out_token);
    EXPECT_EQ(lsid, out_lsid);
    EXPECT_EQ(sid, out_sid);
    EXPECT_EQ(token, out_token);
  }

  void RunErrorParsingTest(const std::string& data,
                           const std::string& error,
                           const std::string& error_url,
                           const std::string& captcha_url,
                           const std::string& captcha_token) {
    std::string out_error;
    std::string out_error_url;
    std::string out_captcha_url;
    std::string out_captcha_token;

    GaiaAuthenticator2::ParseClientLoginFailure(data,
                                                &out_error,
                                                &out_error_url,
                                                &out_captcha_url,
                                                &out_captcha_token);
    EXPECT_EQ(error, out_error);
    EXPECT_EQ(error_url, out_error_url);
    EXPECT_EQ(captcha_url, out_captcha_url);
    EXPECT_EQ(captcha_token, out_captcha_token);
  }

  ResponseCookies cookies_;
  GURL client_login_source_;
  GURL issue_auth_token_source_;
  TestingProfile profile_;
};

class MockGaiaConsumer : public GaiaAuthConsumer {
 public:
  MockGaiaConsumer() {}
  ~MockGaiaConsumer() {}

  MOCK_METHOD1(OnClientLoginSuccess, void(const ClientLoginResult& result));
  MOCK_METHOD2(OnIssueAuthTokenSuccess, void(const std::string& service,
      const std::string& token));
  MOCK_METHOD1(OnClientLoginFailure,
      void(const GoogleServiceAuthError& error));
  MOCK_METHOD2(OnIssueAuthTokenFailure, void(const std::string& service,
      const GoogleServiceAuthError& error));
};

TEST_F(GaiaAuthenticator2Test, ErrorComparator) {
  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError::FromConnectionError(-101);

  GoogleServiceAuthError matching_error =
      GoogleServiceAuthError::FromConnectionError(-101);

  EXPECT_TRUE(expected_error == matching_error);

  expected_error = GoogleServiceAuthError::FromConnectionError(6);

  EXPECT_FALSE(expected_error == matching_error);

  expected_error = GoogleServiceAuthError(GoogleServiceAuthError::NONE);

  EXPECT_FALSE(expected_error == matching_error);

  matching_error = GoogleServiceAuthError(GoogleServiceAuthError::NONE);

  EXPECT_TRUE(expected_error == matching_error);
}

TEST_F(GaiaAuthenticator2Test, LoginNetFailure) {
  int error_no = net::ERR_CONNECTION_RESET;
  URLRequestStatus status(URLRequestStatus::FAILED, error_no);

  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError::FromConnectionError(error_no);

  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnClientLoginFailure(expected_error))
      .Times(1);

  GaiaAuthenticator2 auth(&consumer, std::string(),
      profile_.GetRequestContext());

  auth.OnURLFetchComplete(NULL,
                          client_login_source_,
                          status,
                          0,
                          cookies_,
                          std::string());
}

TEST_F(GaiaAuthenticator2Test, TokenNetFailure) {
  int error_no = net::ERR_CONNECTION_RESET;
  URLRequestStatus status(URLRequestStatus::FAILED, error_no);

  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError::FromConnectionError(error_no);

  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnIssueAuthTokenFailure(_, expected_error))
      .Times(1);

  GaiaAuthenticator2 auth(&consumer, std::string(),
      profile_.GetRequestContext());

  auth.OnURLFetchComplete(NULL,
                          issue_auth_token_source_,
                          status,
                          0,
                          cookies_,
                          std::string());
}


TEST_F(GaiaAuthenticator2Test, LoginDenied) {
  std::string data("Error: NO!");
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);

  GoogleServiceAuthError expected_error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);

  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnClientLoginFailure(expected_error))
      .Times(1);

  GaiaAuthenticator2 auth(&consumer, std::string(),
      profile_.GetRequestContext());
  auth.OnURLFetchComplete(NULL,
                          client_login_source_,
                          status,
                          RC_FORBIDDEN,
                          cookies_,
                          data);
}

TEST_F(GaiaAuthenticator2Test, ParseRequest) {
  RunParsingTest("SID=sid\nLSID=lsid\nAuth=auth\n", "sid", "lsid", "auth");
  RunParsingTest("LSID=lsid\nSID=sid\nAuth=auth\n", "sid", "lsid", "auth");
  RunParsingTest("SID=sid\nLSID=lsid\nAuth=auth", "sid", "lsid", "auth");
  RunParsingTest("SID=sid\nAuth=auth\n", "sid", "", "auth");
  RunParsingTest("LSID=lsid\nAuth=auth\n", "", "lsid", "auth");
  RunParsingTest("\nAuth=auth\n", "", "", "auth");
  RunParsingTest("SID=sid", "sid", "", "");
}

TEST_F(GaiaAuthenticator2Test, ParseErrorRequest) {
  RunErrorParsingTest("Url=U\n"
                      "Error=E\n"
                      "CaptchaToken=T\n"
                      "CaptchaUrl=C\n", "E", "U", "C", "T");
  RunErrorParsingTest("CaptchaToken=T\n"
                      "Error=E\n"
                      "Url=U\n"
                      "CaptchaUrl=C\n", "E", "U", "C", "T");
  RunErrorParsingTest("\n\n\nCaptchaToken=T\n"
                      "\nError=E\n"
                      "\nUrl=U\n"
                      "CaptchaUrl=C\n", "E", "U", "C", "T");
}


TEST_F(GaiaAuthenticator2Test, OnlineLogin) {
  std::string data("SID=sid\nLSID=lsid\nAuth=auth\n");

  GaiaAuthConsumer::ClientLoginResult result;
  result.lsid = "lsid";
  result.sid = "sid";
  result.token = "auth";
  result.data = data;

  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnClientLoginSuccess(result))
      .Times(1);

  GaiaAuthenticator2 auth(&consumer, std::string(),
      profile_.GetRequestContext());
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);
  auth.OnURLFetchComplete(NULL,
                          client_login_source_,
                          status,
                          RC_REQUEST_OK,
                          cookies_,
                          data);
}

TEST_F(GaiaAuthenticator2Test, WorkingIssueAuthToken) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnIssueAuthTokenSuccess(_, "token"))
      .Times(1);

  GaiaAuthenticator2 auth(&consumer, std::string(),
      profile_.GetRequestContext());
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);
  auth.OnURLFetchComplete(NULL,
                          issue_auth_token_source_,
                          status,
                          RC_REQUEST_OK,
                          cookies_,
                          "token");
}

TEST_F(GaiaAuthenticator2Test, CheckTwoFactorResponse) {
  std::string response =
      base::StringPrintf("Error=BadAuthentication\n%s\n",
                         GaiaAuthenticator2::kSecondFactor);
  EXPECT_TRUE(GaiaAuthenticator2::IsSecondFactorSuccess(response));
}

TEST_F(GaiaAuthenticator2Test, CheckNormalErrorCode) {
  std::string response = "Error=BadAuthentication\n";
  EXPECT_FALSE(GaiaAuthenticator2::IsSecondFactorSuccess(response));
}

TEST_F(GaiaAuthenticator2Test, TwoFactorLogin) {
  std::string response = base::StringPrintf("Error=BadAuthentication\n%s\n",
      GaiaAuthenticator2::kSecondFactor);

  GoogleServiceAuthError error =
      GoogleServiceAuthError(GoogleServiceAuthError::TWO_FACTOR);

  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnClientLoginFailure(error))
      .Times(1);

  GaiaAuthenticator2 auth(&consumer, std::string(),
      profile_.GetRequestContext());
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);
  auth.OnURLFetchComplete(NULL,
                          client_login_source_,
                          status,
                          RC_FORBIDDEN,
                          cookies_,
                          response);
}

TEST_F(GaiaAuthenticator2Test, CaptchaParse) {
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);
  std::string data = "Url=http://www.google.com/login/captcha\n"
                     "Error=CaptchaRequired\n"
                     "CaptchaToken=CCTOKEN\n"
                     "CaptchaUrl=Captcha?ctoken=CCTOKEN\n";
  GoogleServiceAuthError error =
      GaiaAuthenticator2::GenerateAuthError(data, status);

  std::string token = "CCTOKEN";
  GURL image_url("http://www.google.com/accounts/Captcha?ctoken=CCTOKEN");
  GURL unlock_url("http://www.google.com/login/captcha");

  EXPECT_EQ(error.state(), GoogleServiceAuthError::CAPTCHA_REQUIRED);
  EXPECT_EQ(error.captcha().token, token);
  EXPECT_EQ(error.captcha().image_url, image_url);
  EXPECT_EQ(error.captcha().unlock_url, unlock_url);
}

TEST_F(GaiaAuthenticator2Test, AccountDeletedError) {
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);
  std::string data = "Error=AccountDeleted\n";
  GoogleServiceAuthError error =
      GaiaAuthenticator2::GenerateAuthError(data, status);
  EXPECT_EQ(error.state(), GoogleServiceAuthError::ACCOUNT_DELETED);
}

TEST_F(GaiaAuthenticator2Test, AccountDisabledError) {
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);
  std::string data = "Error=AccountDisabled\n";
  GoogleServiceAuthError error =
      GaiaAuthenticator2::GenerateAuthError(data, status);
  EXPECT_EQ(error.state(), GoogleServiceAuthError::ACCOUNT_DISABLED);
}

TEST_F(GaiaAuthenticator2Test,ServiceUnavailableError) {
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);
  std::string data = "Error=ServiceUnavailable\n";
  GoogleServiceAuthError error =
      GaiaAuthenticator2::GenerateAuthError(data, status);
  EXPECT_EQ(error.state(), GoogleServiceAuthError::SERVICE_UNAVAILABLE);
}

TEST_F(GaiaAuthenticator2Test, FullLogin) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnClientLoginSuccess(_))
      .Times(1);

  TestingProfile profile;

  MockFactory<MockFetcher> factory;
  URLFetcher::set_factory(&factory);

  GaiaAuthenticator2 auth(&consumer, std::string(),
      profile_.GetRequestContext());
  auth.StartClientLogin("username",
                        "password",
                        "service",
                        std::string(),
                        std::string(),
                        GaiaAuthenticator2::HostedAccountsAllowed);

  URLFetcher::set_factory(NULL);
}

TEST_F(GaiaAuthenticator2Test, FullLoginFailure) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnClientLoginFailure(_))
      .Times(1);

  TestingProfile profile;

  MockFactory<MockFetcher> factory;
  URLFetcher::set_factory(&factory);
  factory.set_success(false);

  GaiaAuthenticator2 auth(&consumer, std::string(),
      profile_.GetRequestContext());
  auth.StartClientLogin("username",
                        "password",
                        "service",
                        std::string(),
                        std::string(),
                        GaiaAuthenticator2::HostedAccountsAllowed);

  URLFetcher::set_factory(NULL);
}

TEST_F(GaiaAuthenticator2Test, ClientFetchPending) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnClientLoginSuccess(_))
      .Times(1);

  TestingProfile profile;
  TestURLFetcherFactory factory;
  URLFetcher::set_factory(&factory);

  GaiaAuthenticator2 auth(&consumer, std::string(),
      profile_.GetRequestContext());
  auth.StartClientLogin("username",
                        "password",
                        "service",
                        std::string(),
                        std::string(),
                        GaiaAuthenticator2::HostedAccountsAllowed);

  URLFetcher::set_factory(NULL);
  EXPECT_TRUE(auth.HasPendingFetch());
  auth.OnURLFetchComplete(NULL,
                          client_login_source_,
                          URLRequestStatus(URLRequestStatus::SUCCESS, 0),
                          RC_REQUEST_OK,
                          cookies_,
                          "SID=sid\nLSID=lsid\nAuth=auth\n");
  EXPECT_FALSE(auth.HasPendingFetch());
}

TEST_F(GaiaAuthenticator2Test, FullTokenSuccess) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnIssueAuthTokenSuccess("service", "token"))
      .Times(1);

  TestingProfile profile;
  TestURLFetcherFactory factory;
  URLFetcher::set_factory(&factory);

  GaiaAuthenticator2 auth(&consumer, std::string(),
      profile_.GetRequestContext());
  auth.StartIssueAuthToken("sid", "lsid", "service");

  URLFetcher::set_factory(NULL);
  EXPECT_TRUE(auth.HasPendingFetch());
  auth.OnURLFetchComplete(NULL,
                          issue_auth_token_source_,
                          URLRequestStatus(URLRequestStatus::SUCCESS, 0),
                          RC_REQUEST_OK,
                          cookies_,
                          "token");
  EXPECT_FALSE(auth.HasPendingFetch());
}

TEST_F(GaiaAuthenticator2Test, FullTokenFailure) {
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnIssueAuthTokenFailure("service", _))
      .Times(1);

  TestingProfile profile;
  TestURLFetcherFactory factory;
  URLFetcher::set_factory(&factory);

  GaiaAuthenticator2 auth(&consumer, std::string(),
      profile_.GetRequestContext());
  auth.StartIssueAuthToken("sid", "lsid", "service");

  URLFetcher::set_factory(NULL);
  EXPECT_TRUE(auth.HasPendingFetch());
  auth.OnURLFetchComplete(NULL,
                          issue_auth_token_source_,
                          URLRequestStatus(URLRequestStatus::SUCCESS, 0),
                          RC_FORBIDDEN,
                          cookies_,
                          "");
  EXPECT_FALSE(auth.HasPendingFetch());
}
