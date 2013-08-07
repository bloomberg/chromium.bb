// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/managed_mode/managed_user_refresh_token_fetcher.h"
#include "chrome/browser/signin/oauth2_token_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kManagedUserId[] = "abcdef";
const char kDeviceName[] = "Compy";

const char kAccessToken[] = "accesstoken";
const char kAuthorizationCode[] = "authorizationcode";
const char kManagedUserToken[] = "managedusertoken";

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

// MockOAuth2TokenService ---------------------------------------------

class MockOAuth2TokenService : public OAuth2TokenService {
 public:
  class Request : public OAuth2TokenService::Request {
   public:
    Request(const OAuth2TokenService::ScopeSet& scopes,
            OAuth2TokenService::Consumer* consumer,
            MockOAuth2TokenService* owner);
    virtual ~Request();

    void Succeed();
    void Fail(GoogleServiceAuthError::State error);

    const OAuth2TokenService::ScopeSet& scopes() const { return scopes_; }

   private:
    OAuth2TokenService::ScopeSet scopes_;

    OAuth2TokenService::Consumer* consumer_;

    MockOAuth2TokenService* owner_;
  };

  MockOAuth2TokenService();
  virtual ~MockOAuth2TokenService();

  Request* request() const { return request_; }

  void ClearRequest(Request* request);

 private:
  // OAuth2TokenService overrides:
  virtual scoped_ptr<OAuth2TokenService::Request> StartRequest(
     const OAuth2TokenService::ScopeSet& scopes,
      OAuth2TokenService::Consumer* consumer) OVERRIDE;
  virtual std::string GetRefreshToken() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE {
    return NULL;
  }

  Request* request_;

  DISALLOW_COPY_AND_ASSIGN(MockOAuth2TokenService);
};

MockOAuth2TokenService::Request::Request(
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenService::Consumer* consumer,
    MockOAuth2TokenService* owner)
    : scopes_(scopes),
      consumer_(consumer),
      owner_(owner) {}

MockOAuth2TokenService::Request::~Request() {
  owner_->ClearRequest(this);
}

void MockOAuth2TokenService::Request::Succeed() {
  base::Time expiration_date = base::Time::Now() +
                               base::TimeDelta::FromHours(1);
  consumer_->OnGetTokenSuccess(this, kAccessToken, expiration_date);
}

void MockOAuth2TokenService::Request::Fail(
    GoogleServiceAuthError::State error) {
  consumer_->OnGetTokenFailure(this, GoogleServiceAuthError(error));
}

MockOAuth2TokenService::MockOAuth2TokenService() : request_(NULL) {}

MockOAuth2TokenService::~MockOAuth2TokenService() {
  EXPECT_FALSE(request_);
}

void MockOAuth2TokenService::ClearRequest(
    MockOAuth2TokenService::Request* request) {
  if (request_ == request)
    request_ = NULL;
}

scoped_ptr<OAuth2TokenService::Request> MockOAuth2TokenService::StartRequest(
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenService::Consumer* consumer) {
  scoped_ptr<Request> request(new Request(scopes, consumer, this));
  request_ = request.get();
  return request.PassAs<OAuth2TokenService::Request>();
}

std::string MockOAuth2TokenService::GetRefreshToken() {
  NOTREACHED();
  return std::string();
}

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

}  // namespace

class ManagedUserRefreshTokenFetcherTest : public testing::Test {
 public:
  ManagedUserRefreshTokenFetcherTest();
  virtual ~ManagedUserRefreshTokenFetcherTest() {}

 protected:
  void StartFetching();

  MockOAuth2TokenService::Request* GetOAuth2TokenServiceRequest();
  net::TestURLFetcher* GetIssueTokenRequest();
  net::TestURLFetcher* GetRefreshTokenRequest();

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
  MockOAuth2TokenService oauth2_token_service_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  scoped_ptr<ManagedUserRefreshTokenFetcher> token_fetcher_;

  GoogleServiceAuthError error_;
  std::string token_;
  base::WeakPtrFactory<ManagedUserRefreshTokenFetcherTest> weak_ptr_factory_;
};

ManagedUserRefreshTokenFetcherTest::ManagedUserRefreshTokenFetcherTest()
    : token_fetcher_(
          ManagedUserRefreshTokenFetcher::Create(&oauth2_token_service_,
                                          profile_.GetRequestContext())),
      error_(GoogleServiceAuthError::NONE),
      weak_ptr_factory_(this) {}

void ManagedUserRefreshTokenFetcherTest::StartFetching() {
  token_fetcher_->Start(kManagedUserId, kDeviceName,
                        base::Bind(
                            &ManagedUserRefreshTokenFetcherTest::OnTokenFetched,
                            weak_ptr_factory_.GetWeakPtr()));
}

MockOAuth2TokenService::Request*
ManagedUserRefreshTokenFetcherTest::GetOAuth2TokenServiceRequest() {
  MockOAuth2TokenService::Request* request = oauth2_token_service_.request();

  OAuth2TokenService::ScopeSet scopes = request->scopes();
  EXPECT_EQ(1u, scopes.size());
  EXPECT_EQ(1u, scopes.count(GaiaUrls::GetInstance()->oauth1_login_scope()));
  return request;
}

net::TestURLFetcher*
ManagedUserRefreshTokenFetcherTest::GetIssueTokenRequest() {
  net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(1);
  if (!url_fetcher)
    return NULL;

  EXPECT_EQ(GaiaUrls::GetInstance()->oauth2_issue_token_url(),
            url_fetcher->GetOriginalURL().spec());
  std::string access_token;
  net::HttpRequestHeaders headers;
  url_fetcher->GetExtraRequestHeaders(&headers);
  EXPECT_TRUE(headers.GetHeader("Authorization", &access_token));
  EXPECT_EQ(std::string("Bearer ") + kAccessToken, access_token);
  const std::string upload_data = url_fetcher->upload_data();
  std::string managed_user_id;
  EXPECT_TRUE(GetValueForKey(upload_data, "profile_id", &managed_user_id));
  EXPECT_EQ(kManagedUserId, managed_user_id);
  std::string device_name;
  EXPECT_TRUE(GetValueForKey(upload_data, "device_name", &device_name));
  EXPECT_EQ(kDeviceName, device_name);
  return url_fetcher;
}

net::TestURLFetcher*
ManagedUserRefreshTokenFetcherTest::GetRefreshTokenRequest() {
  net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(
      gaia::GaiaOAuthClient::kUrlFetcherId);
  if (!url_fetcher)
    return NULL;

  EXPECT_EQ(GaiaUrls::GetInstance()->oauth2_token_url(),
            url_fetcher->GetOriginalURL().spec());
  std::string auth_code;
  EXPECT_TRUE(GetValueForKey(url_fetcher->upload_data(), "code", &auth_code));
  EXPECT_EQ(kAuthorizationCode, auth_code);
  return url_fetcher;
}

void ManagedUserRefreshTokenFetcherTest::MakeIssueTokenRequestSucceed() {
  SendResponse(GetIssueTokenRequest(),
               base::StringPrintf(kIssueTokenResponseFormat,
                                  kAuthorizationCode));
}

void ManagedUserRefreshTokenFetcherTest::MakeRefreshTokenFetchSucceed() {
  SendResponse(GetRefreshTokenRequest(),
               base::StringPrintf(kGetRefreshTokenResponseFormat,
                                  kManagedUserToken));
}

void ManagedUserRefreshTokenFetcherTest::Reset() {
  token_fetcher_.reset();
}

void ManagedUserRefreshTokenFetcherTest::OnTokenFetched(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  error_ = error;
  token_ = token;
}

// Tests --------------------------------------------------------

TEST_F(ManagedUserRefreshTokenFetcherTest, Success) {
  StartFetching();
  GetOAuth2TokenServiceRequest()->Succeed();
  MakeIssueTokenRequestSucceed();
  MakeRefreshTokenFetchSucceed();

  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_EQ(kManagedUserToken, token());
}

TEST_F(ManagedUserRefreshTokenFetcherTest, ExpiredAccessToken) {
  StartFetching();
  GetOAuth2TokenServiceRequest()->Succeed();
  SetHttpError(GetIssueTokenRequest(), net::HTTP_UNAUTHORIZED);
  GetOAuth2TokenServiceRequest()->Succeed();
  MakeIssueTokenRequestSucceed();
  MakeRefreshTokenFetchSucceed();

  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_EQ(kManagedUserToken, token());
}

TEST_F(ManagedUserRefreshTokenFetcherTest, ExpiredAccessTokenRetry) {
  // If we get a 401 error for the second time, we should give up instead of
  // retrying again.
  StartFetching();
  GetOAuth2TokenServiceRequest()->Succeed();
  SetHttpError(GetIssueTokenRequest(), net::HTTP_UNAUTHORIZED);
  GetOAuth2TokenServiceRequest()->Succeed();
  SetHttpError(GetIssueTokenRequest(), net::HTTP_UNAUTHORIZED);

  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED, error().state());
  EXPECT_EQ(net::ERR_FAILED, error().network_error());
  EXPECT_EQ(std::string(), token());
}

TEST_F(ManagedUserRefreshTokenFetcherTest, MalformedIssueTokenResponse) {
  StartFetching();
  GetOAuth2TokenServiceRequest()->Succeed();
  SendResponse(GetIssueTokenRequest(), "choke");

  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED, error().state());
  EXPECT_EQ(net::ERR_INVALID_RESPONSE, error().network_error());
  EXPECT_EQ(std::string(), token());
}

TEST_F(ManagedUserRefreshTokenFetcherTest, FetchAccessTokenFailure) {
  StartFetching();
  GetOAuth2TokenServiceRequest()->Fail(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);

  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, error().state());
  EXPECT_EQ(std::string(), token());
}

TEST_F(ManagedUserRefreshTokenFetcherTest, IssueTokenNetworkError) {
  StartFetching();
  GetOAuth2TokenServiceRequest()->Succeed();
  SetNetworkError(GetIssueTokenRequest(), net::ERR_SSL_PROTOCOL_ERROR);

  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED, error().state());
  EXPECT_EQ(net::ERR_SSL_PROTOCOL_ERROR, error().network_error());
  EXPECT_EQ(std::string(), token());
}

TEST_F(ManagedUserRefreshTokenFetcherTest, FetchRefreshTokenNetworkError) {
  StartFetching();
  GetOAuth2TokenServiceRequest()->Succeed();
  MakeIssueTokenRequestSucceed();
  SetNetworkError(GetRefreshTokenRequest(), net::ERR_CONNECTION_REFUSED);
  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  SetNetworkError(GetRefreshTokenRequest(), net::ERR_CONNECTION_REFUSED);

  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED, error().state());
  EXPECT_EQ(net::ERR_FAILED, error().network_error());
  EXPECT_EQ(std::string(), token());
}

TEST_F(ManagedUserRefreshTokenFetcherTest,
       FetchRefreshTokenTransientNetworkError) {
  StartFetching();
  GetOAuth2TokenServiceRequest()->Succeed();
  MakeIssueTokenRequestSucceed();
  SetNetworkError(GetRefreshTokenRequest(), net::ERR_CONNECTION_REFUSED);

  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  MakeRefreshTokenFetchSucceed();

  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_EQ(kManagedUserToken, token());
}

TEST_F(ManagedUserRefreshTokenFetcherTest, FetchRefreshTokenBadRequest) {
  StartFetching();
  GetOAuth2TokenServiceRequest()->Succeed();
  MakeIssueTokenRequestSucceed();
  SetHttpError(GetRefreshTokenRequest(), net::HTTP_BAD_REQUEST);

  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED, error().state());
  EXPECT_EQ(net::ERR_FAILED, error().network_error());
  EXPECT_EQ(std::string(), token());
}

TEST_F(ManagedUserRefreshTokenFetcherTest, CancelWhileFetchingAccessToken) {
  StartFetching();
  Reset();

  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_EQ(std::string(), token());
}

TEST_F(ManagedUserRefreshTokenFetcherTest, CancelWhileCallingIssueToken) {
  StartFetching();
  GetOAuth2TokenServiceRequest()->Succeed();
  Reset();

  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_EQ(std::string(), token());
}

TEST_F(ManagedUserRefreshTokenFetcherTest, CancelWhileFetchingRefreshToken) {
  StartFetching();
  GetOAuth2TokenServiceRequest()->Succeed();
  MakeIssueTokenRequestSucceed();
  Reset();

  EXPECT_EQ(GoogleServiceAuthError::NONE, error().state());
  EXPECT_EQ(std::string(), token());
}
