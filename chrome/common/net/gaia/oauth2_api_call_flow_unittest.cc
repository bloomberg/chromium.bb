// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for OAuth2MintTokenFlow.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/net/gaia/oauth2_access_token_consumer.h"
#include "chrome/common/net/gaia/oauth2_access_token_fetcher.h"
#include "chrome/common/net/gaia/oauth2_api_call_flow.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/url_fetcher.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "content/public/common/url_fetcher_factory.h"
#include "content/test/test_url_fetcher_factory.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::URLFetcher;
using content::URLFetcherDelegate;
using content::URLFetcherFactory;
using net::HttpRequestHeaders;
using net::URLRequestStatus;
using testing::_;
using testing::Return;

namespace {
static std::string CreateBody() {
  return "some body";
}

static GURL CreateApiUrl() {
  return GURL("https://www.googleapis.com/someapi");
}

static std::vector<std::string> CreateTestScopes() {
  std::vector<std::string> scopes;
  scopes.push_back("scope1");
  scopes.push_back("scope2");
  return scopes;
}

}  // namespace

class MockUrlFetcherFactory : public ScopedURLFetcherFactory,
                              public URLFetcherFactory {
 public:
  MockUrlFetcherFactory()
      : ScopedURLFetcherFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  }
  virtual ~MockUrlFetcherFactory() {}

  MOCK_METHOD4(
      CreateURLFetcher,
      URLFetcher* (int id,
                   const GURL& url,
                   URLFetcher::RequestType request_type,
                   URLFetcherDelegate* d));
};

class MockAccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  MockAccessTokenFetcher(OAuth2AccessTokenConsumer* consumer,
                         net::URLRequestContextGetter* getter)
      : OAuth2AccessTokenFetcher(consumer, getter) {}
  ~MockAccessTokenFetcher() {}

  MOCK_METHOD4(Start,
               void (const std::string& client_id,
                     const std::string& client_secret,
                     const std::string& refresh_token,
                     const std::vector<std::string>& scopes));
};

class MockApiCallFlow : public OAuth2ApiCallFlow {
 public:
  MockApiCallFlow(net::URLRequestContextGetter* context,
                  const std::string& refresh_token,
                  const std::string& access_token,
                  const std::vector<std::string>& scopes)
      : OAuth2ApiCallFlow(context, refresh_token, access_token, scopes) {}
  ~MockApiCallFlow() {}

  MOCK_METHOD0(CreateApiCallUrl, GURL ());
  MOCK_METHOD0(CreateApiCallBody, std::string ());
  MOCK_METHOD1(ProcessApiCallSuccess,
      void (const net::URLFetcher* source));
  MOCK_METHOD1(ProcessApiCallFailure,
      void (const net::URLFetcher* source));
  MOCK_METHOD1(ProcessNewAccessToken,
      void (const std::string& access_token));
  MOCK_METHOD1(ProcessMintAccessTokenFailure,
      void (const GoogleServiceAuthError& error));
  MOCK_METHOD0(CreateAccessTokenFetcher, OAuth2AccessTokenFetcher* ());
};

class OAuth2ApiCallFlowTest : public testing::Test {
 public:
  OAuth2ApiCallFlowTest() {}
  virtual ~OAuth2ApiCallFlowTest() {}

 protected:
  void SetupAccessTokenFetcher(
      const std::string& rt, const std::vector<std::string>& scopes) {
    EXPECT_CALL(*access_token_fetcher_,
        Start(GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
              GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
              rt, scopes))
        .Times(1);
    EXPECT_CALL(*flow_, CreateAccessTokenFetcher())
        .WillOnce(Return(access_token_fetcher_.release()));
  }

  TestURLFetcher* CreateURLFetcher(
      const GURL& url, bool fetch_succeeds,
      int response_code, const std::string& body) {
    TestURLFetcher* url_fetcher = new TestURLFetcher(0, url, flow_.get());
    URLRequestStatus::Status status =
        fetch_succeeds ? URLRequestStatus::SUCCESS : URLRequestStatus::FAILED;
    url_fetcher->set_status(URLRequestStatus(status, 0));

    if (response_code != 0)
      url_fetcher->set_response_code(response_code);

    if (!body.empty())
      url_fetcher->SetResponseString(body);

    return url_fetcher;
  }

  void CreateFlow(const std::string& refresh_token,
                  const std::string& access_token,
                  const std::vector<std::string>& scopes) {
    flow_.reset(new MockApiCallFlow(
        profile_.GetRequestContext(),
        refresh_token,
        access_token,
        scopes));
    access_token_fetcher_.reset(new MockAccessTokenFetcher(
        flow_.get(), profile_.GetRequestContext()));
  }

  TestURLFetcher* SetupApiCall(bool succeeds, net::HttpStatusCode status) {
    std::string body(CreateBody());
    GURL url(CreateApiUrl());
    EXPECT_CALL(*flow_, CreateApiCallBody()).WillOnce(Return(body));
    EXPECT_CALL(*flow_, CreateApiCallUrl()).WillOnce(Return(url));
    TestURLFetcher* url_fetcher = CreateURLFetcher(
        url, succeeds, status, "");
    EXPECT_CALL(factory_, CreateURLFetcher(_, url, _, _))
        .WillOnce(Return(url_fetcher));
    return url_fetcher;
  }

  MockUrlFetcherFactory factory_;
  scoped_ptr<MockApiCallFlow> flow_;
  scoped_ptr<MockAccessTokenFetcher> access_token_fetcher_;
  TestingProfile profile_;
};

TEST_F(OAuth2ApiCallFlowTest, FirstApiCallSucceeds) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());

  CreateFlow(rt, at, scopes);
  TestURLFetcher* url_fetcher = SetupApiCall(true, net::HTTP_OK);
  EXPECT_CALL(*flow_, ProcessApiCallSuccess(url_fetcher));
  flow_->Start();
  flow_->OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2ApiCallFlowTest, SecondApiCallSucceeds) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());

  CreateFlow(rt, at, scopes);
  TestURLFetcher* url_fetcher1 = SetupApiCall(true, net::HTTP_UNAUTHORIZED);
  flow_->Start();
  SetupAccessTokenFetcher(rt, scopes);
  flow_->OnURLFetchComplete(url_fetcher1);
  TestURLFetcher* url_fetcher2 = SetupApiCall(true, net::HTTP_OK);
  EXPECT_CALL(*flow_, ProcessApiCallSuccess(url_fetcher2));
  flow_->OnGetTokenSuccess(at);
  flow_->OnURLFetchComplete(url_fetcher2);
}

TEST_F(OAuth2ApiCallFlowTest, SecondApiCallFails) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());

  CreateFlow(rt, at, scopes);
  TestURLFetcher* url_fetcher1 = SetupApiCall(true, net::HTTP_UNAUTHORIZED);
  flow_->Start();
  SetupAccessTokenFetcher(rt, scopes);
  flow_->OnURLFetchComplete(url_fetcher1);
  TestURLFetcher* url_fetcher2 = SetupApiCall(false, net::HTTP_UNAUTHORIZED);
  EXPECT_CALL(*flow_, ProcessApiCallFailure(url_fetcher2));
  flow_->OnGetTokenSuccess(at);
  flow_->OnURLFetchComplete(url_fetcher2);
}

TEST_F(OAuth2ApiCallFlowTest, NewTokenGenerationFails) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());

  CreateFlow(rt, at, scopes);
  TestURLFetcher* url_fetcher = SetupApiCall(true, net::HTTP_UNAUTHORIZED);
  flow_->Start();
  SetupAccessTokenFetcher(rt, scopes);
  flow_->OnURLFetchComplete(url_fetcher);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(*flow_, ProcessMintAccessTokenFailure(error));
  flow_->OnGetTokenFailure(error);
}

TEST_F(OAuth2ApiCallFlowTest, EmptyAccessTokenFirstApiCallSucceeds) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());

  CreateFlow(rt, "", scopes);
  SetupAccessTokenFetcher(rt, scopes);
  TestURLFetcher* url_fetcher = SetupApiCall(true, net::HTTP_OK);
  EXPECT_CALL(*flow_, ProcessApiCallSuccess(url_fetcher));
  flow_->Start();
  flow_->OnGetTokenSuccess(at);
  flow_->OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2ApiCallFlowTest, EmptyAccessTokenApiCallFails) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());

  CreateFlow(rt, "", scopes);
  SetupAccessTokenFetcher(rt, scopes);
  TestURLFetcher* url_fetcher = SetupApiCall(false, net::HTTP_BAD_GATEWAY);
  EXPECT_CALL(*flow_, ProcessApiCallFailure(url_fetcher));
  flow_->Start();
  flow_->OnGetTokenSuccess(at);
  flow_->OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2ApiCallFlowTest, EmptyAccessTokenNewTokenGenerationFails) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());

  CreateFlow(rt, "", scopes);
  SetupAccessTokenFetcher(rt, scopes);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(*flow_, ProcessMintAccessTokenFailure(error));
  flow_->Start();
  flow_->OnGetTokenFailure(error);
}

TEST_F(OAuth2ApiCallFlowTest, CreateURLFetcher) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());
  std::string body = CreateBody();
  GURL url(CreateApiUrl());

  CreateFlow(rt, at, scopes);
  scoped_ptr<TestURLFetcher> url_fetcher(SetupApiCall(true, net::HTTP_OK));
  flow_->CreateURLFetcher();
  HttpRequestHeaders headers;
  url_fetcher->GetExtraRequestHeaders(&headers);
  std::string auth_header;
  EXPECT_TRUE(headers.GetHeader("Authorization", &auth_header));
  EXPECT_EQ("Bearer access_token", auth_header);
  EXPECT_EQ(url, url_fetcher->GetOriginalURL());
  EXPECT_EQ(body, url_fetcher->upload_data());
}
