// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/supervised_user/supervised_user_refresh_token_fetcher.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAccountId[] = "account_id";
const char kDeviceId[] = "device_id";
const char kDeviceName[] = "Compy";
const char kSupervisedUserId[] = "abcdef";

const char kAccessToken[] = "accesstoken";
const char kAuthorizationCode[] = "authorizationcode";
const char kSupervisedUserToken[] = "supervisedusertoken";
const char kOAuth2RefreshToken[] = "refreshtoken";

const char kIssueTokenResponseFormat[] =
    "{"
    "  \"code\": \"%s\""
    "}";

const char kGetRefreshTokenResponseFormat[] =
    "{"
    "  \"access_token\": \"<ignored>\","
    "  \"expires_in\": 12345,"
    "  \"refresh_token\": \"%s\""
    "}";

// Utility methods --------------------------------------------------

// Slightly hacky way to extract a value from a URL-encoded POST request body.
bool GetValueForKey(const std::string& encoded_string,
                    const std::string& key,
                    std::string* value) {
  GURL url("http://example.com/?" + encoded_string);
  return net::GetValueForKeyInQuery(url, key, value);
}

void SendResponse(net::TestURLFetcher* url_fetcher,
                  const std::string& response) {
  url_fetcher->set_status(
      net::URLRequestStatus(net::URLRequestStatus::SUCCESS, 0));
  url_fetcher->set_response_code(net::HTTP_OK);
  url_fetcher->SetResponseString(response);
  url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
}

void SetNetworkError(net::TestURLFetcher* url_fetcher, int error) {
  url_fetcher->set_status(
      net::URLRequestStatus(net::URLRequestStatus::FAILED, error));
  url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
}

void SetHttpError(net::TestURLFetcher* url_fetcher, int error) {
  url_fetcher->set_status(net::URLRequestStatus());
  url_fetcher->set_response_code(error);
  url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
}

void VerifyTokenRequest(
    std::vector<FakeProfileOAuth2TokenService::PendingRequest> requests) {
  ASSERT_EQ(1u, requests.size());
  EXPECT_EQ(1u, requests[0].scopes.size());
  EXPECT_EQ(1u, requests[0].scopes.count(GaiaConstants::kOAuth1LoginScope));
}

}  // namespace

class SupervisedUserRefreshTokenFetcherTest : public testing::Test {
 public:
  SupervisedUserRefreshTokenFetcherTest();
  virtual ~SupervisedUserRefreshTokenFetcherTest() {}

 protected:
  void StartFetching();
  void StartFetchingWithDeviceId(const std::string& device_id);

  net::TestURLFetcher* GetIssueTokenRequest();
  net::TestURLFetcher* GetRefreshTokenRequest();

  void MakeOAuth2TokenServiceRequestSucceed();
  void MakeOAuth2TokenServiceRequestFail(GoogleServiceAuthError::State error);
  void MakeIssueTokenRequestSucceed();
  void MakeRefreshTokenFetchSucceed();

  void Reset();

  const GoogleServiceAuthError& error() const { return error_; }
  const std::string& token() const { return token_; }

 private:
  void OnTokenFetched(const GoogleServiceAuthError& error,
                      const std::string& token);

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  FakeProfileOAuth2TokenService oauth2_token_service_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  scoped_ptr<SupervisedUserRefreshTokenFetcher> token_fetcher_;
  std::string device_id_;

  GoogleServiceAuthError error_;
  std::string token_;
  base::WeakPtrFactory<SupervisedUserRefreshTokenFetcherTest> weak_ptr_factory_;
};

SupervisedUserRefreshTokenFetcherTest::SupervisedUserRefreshTokenFetcherTest()
    : error_(GoogleServiceAuthError::NONE),
      weak_ptr_factory_(this) {}

void SupervisedUserRefreshTokenFetcherTest::StartFetching() {
  StartFetchingWithDeviceId(std::string());
}

void SupervisedUserRefreshTokenFetcherTest::StartFetchingWithDeviceId(
    const std::string& device_id) {
  device_id_ = device_id;
  oauth2_token_service_.IssueRefreshToken(kOAuth2RefreshToken);
  token_fetcher_ = SupervisedUserRefreshTokenFetcher::Create(
      &oauth2_token_service_,
      kAccountId,
      device_id_,
      profile_.GetRequestContext()).Pass();
  token_fetcher_->Start(
      kSupervisedUserId,
      kDeviceName,
      base::Bind(
          &SupervisedUserRefreshTokenFetcherTest::OnTokenFetched,
          weak_ptr_factory_.GetWeakPtr()));
}

net::TestURLFetcher*
SupervisedUserRefreshTokenFetcherTest::GetIssueTokenRequest() {
  net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(1);
  if (!url_fetcher)
    return NULL;

  EXPECT_EQ(GaiaUrls::GetInstance()->oauth2_issue_token_url(),
            url_fetcher->GetOriginalURL());
  std::string access_token;
  net::HttpRequestHeaders headers;
  url_fetcher->GetExtraRequestHeaders(&headers);
  EXPECT_TRUE(headers.GetHeader("Authorization", &access_token));
  EXPECT_EQ(std::string("Bearer ") + kAccessToken, access_token);
  const std::string upload_data = url_fetcher->upload_data();
  std::string supervised_user_id;
  EXPECT_TRUE(GetValueForKey(upload_data, "profile_id", &supervised_user_id));
  EXPECT_EQ(kSupervisedUserId, supervised_user_id);
  std::string device_name;
  EXPECT_TRUE(GetValueForKey(upload_data, "device_name", &device_name));
  EXPECT_EQ(kDeviceName, device_name);
  std::string device_id;
  EXPECT_EQ(!device_id_.empty(),
            GetValueForKey(upload_data, "device_id", &device_id));
  EXPECT_EQ(device_id_, device_id);
  return url_fetcher;
}

net::TestURLFetcher*
SupervisedUserRefreshTokenFetcherTest::GetRefreshTokenRequest() {
  net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(
      gaia::GaiaOAuthClient::kUrlFetcherId);
  if (!url_fetcher)
    return NULL;

  EXPECT_EQ(GaiaUrls::GetInstance()->oauth2_token_url(),
            url_fetcher->GetOriginalURL());
  std::string auth_code;
  EXPECT_TRUE(GetValueForKey(url_fetcher->upload_data(), "code", &auth_code));
  EXPECT_EQ(kAuthorizationCode, auth_code);
  return url_fetcher;
}

void
SupervisedUserRefreshTokenFetcherTest::MakeOAuth2TokenServiceRequestSucceed() {
  std::vector<FakeProfileOAuth2TokenService::PendingRequest> requests =
      oauth2_token_service_.GetPendingRequests();
  VerifyTokenRequest(requests);
  base::Time expiration_date = base::Time::Now() +
                               base::TimeDelta::FromHours(1);
  oauth2_token_service_.IssueTokenForScope(requests[0].scopes,
                                           kAccessToken,
                                           expiration_date);
}

void
SupervisedUserRefreshTokenFetcherTest::MakeOAuth2TokenServiceRequestFail(
    GoogleServiceAuthError::State error) {
  std::vector<FakeProfileOAuth2TokenService::PendingRequest> requests =
      oauth2_token_service_.GetPendingRequests();
  VerifyTokenRequest(requests);
  oauth2_token_service_.IssueErrorForScope(requests[0].scopes,
                                           GoogleServiceAuthError(error));
}

void SupervisedUserRefreshTokenFetcherTest::MakeIssueTokenRequestSucceed() {
  SendResponse(GetIssueTokenRequest(),
               base::StringPrintf(kIssueTokenResponseFormat,
                                  kAuthorizationCode));
}

void SupervisedUserRefreshTokenFetcherTest::MakeRefreshTokenFetchSucceed() {
  SendResponse(GetRefreshTokenRequest(),
               base::StringPrintf(kGetRefreshTokenResponseFormat,
                                  kSupervisedUserToken));
}

void SupervisedUserRefreshTokenFetcherTest::Reset() {
  token_fetcher_.reset();
}

void SupervisedUserRefreshTokenFetcherTest::OnTokenFetched(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  error_ = error;
  token_ = token;
}

// Tests --------------------------------------------------------

TEST_F(SupervisedUserRefreshTokenFetcherTest, Success) {
  StartFetching();
  MakeOAuth2TokenServiceRequestSucceed();
  MakeIssueTokenRequestSucceed();
  MakeRefreshTokenFetchSucceed();

  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_EQ(kSupervisedUserToken, token());
}

TEST_F(SupervisedUserRefreshTokenFetcherTest, Success_WithDeviceId) {
  StartFetchingWithDeviceId(kDeviceId);
  MakeOAuth2TokenServiceRequestSucceed();
  MakeIssueTokenRequestSucceed();
  MakeRefreshTokenFetchSucceed();

  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_EQ(kSupervisedUserToken, token());
}

TEST_F(SupervisedUserRefreshTokenFetcherTest, ExpiredAccessToken) {
  StartFetching();
  MakeOAuth2TokenServiceRequestSucceed();
  SetHttpError(GetIssueTokenRequest(), net::HTTP_UNAUTHORIZED);
  MakeOAuth2TokenServiceRequestSucceed();
  MakeIssueTokenRequestSucceed();
  MakeRefreshTokenFetchSucceed();

  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_EQ(kSupervisedUserToken, token());
}

TEST_F(SupervisedUserRefreshTokenFetcherTest, ExpiredAccessTokenRetry) {
  // If we get a 401 error for the second time, we should give up instead of
  // retrying again.
  StartFetching();
  MakeOAuth2TokenServiceRequestSucceed();
  SetHttpError(GetIssueTokenRequest(), net::HTTP_UNAUTHORIZED);
  MakeOAuth2TokenServiceRequestSucceed();
  SetHttpError(GetIssueTokenRequest(), net::HTTP_UNAUTHORIZED);

  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED, error().state());
  EXPECT_EQ(net::ERR_FAILED, error().network_error());
  EXPECT_EQ(std::string(), token());
}

TEST_F(SupervisedUserRefreshTokenFetcherTest, MalformedIssueTokenResponse) {
  StartFetching();
  MakeOAuth2TokenServiceRequestSucceed();
  SendResponse(GetIssueTokenRequest(), "choke");

  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED, error().state());
  EXPECT_EQ(net::ERR_INVALID_RESPONSE, error().network_error());
  EXPECT_EQ(std::string(), token());
}

TEST_F(SupervisedUserRefreshTokenFetcherTest, FetchAccessTokenFailure) {
  StartFetching();
  MakeOAuth2TokenServiceRequestFail(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);

  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, error().state());
  EXPECT_EQ(std::string(), token());
}

TEST_F(SupervisedUserRefreshTokenFetcherTest, IssueTokenNetworkError) {
  StartFetching();
  MakeOAuth2TokenServiceRequestSucceed();
  SetNetworkError(GetIssueTokenRequest(), net::ERR_SSL_PROTOCOL_ERROR);

  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED, error().state());
  EXPECT_EQ(net::ERR_SSL_PROTOCOL_ERROR, error().network_error());
  EXPECT_EQ(std::string(), token());
}

TEST_F(SupervisedUserRefreshTokenFetcherTest, FetchRefreshTokenNetworkError) {
  StartFetching();
  MakeOAuth2TokenServiceRequestSucceed();
  MakeIssueTokenRequestSucceed();
  SetNetworkError(GetRefreshTokenRequest(), net::ERR_CONNECTION_REFUSED);
  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  SetNetworkError(GetRefreshTokenRequest(), net::ERR_CONNECTION_REFUSED);

  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED, error().state());
  EXPECT_EQ(net::ERR_FAILED, error().network_error());
  EXPECT_EQ(std::string(), token());
}

TEST_F(SupervisedUserRefreshTokenFetcherTest,
       FetchRefreshTokenTransientNetworkError) {
  StartFetching();
  MakeOAuth2TokenServiceRequestSucceed();
  MakeIssueTokenRequestSucceed();
  SetNetworkError(GetRefreshTokenRequest(), net::ERR_CONNECTION_REFUSED);

  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  MakeRefreshTokenFetchSucceed();

  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_EQ(kSupervisedUserToken, token());
}

TEST_F(SupervisedUserRefreshTokenFetcherTest, FetchRefreshTokenBadRequest) {
  StartFetching();
  MakeOAuth2TokenServiceRequestSucceed();
  MakeIssueTokenRequestSucceed();
  SetHttpError(GetRefreshTokenRequest(), net::HTTP_BAD_REQUEST);

  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED, error().state());
  EXPECT_EQ(net::ERR_FAILED, error().network_error());
  EXPECT_EQ(std::string(), token());
}

TEST_F(SupervisedUserRefreshTokenFetcherTest, CancelWhileFetchingAccessToken) {
  StartFetching();
  Reset();

  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_EQ(std::string(), token());
}

TEST_F(SupervisedUserRefreshTokenFetcherTest, CancelWhileCallingIssueToken) {
  StartFetching();
  MakeOAuth2TokenServiceRequestSucceed();
  Reset();

  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_EQ(std::string(), token());
}

TEST_F(SupervisedUserRefreshTokenFetcherTest, CancelWhileFetchingRefreshToken) {
  StartFetching();
  MakeOAuth2TokenServiceRequestSucceed();
  MakeIssueTokenRequestSucceed();
  Reset();

  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_EQ(std::string(), token());
}
