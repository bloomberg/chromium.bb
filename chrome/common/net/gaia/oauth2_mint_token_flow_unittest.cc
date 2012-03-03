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
#include "chrome/common/net/gaia/oauth2_mint_token_consumer.h"
#include "chrome/common/net/gaia/oauth2_mint_token_fetcher.h"
#include "chrome/common/net/gaia/oauth2_mint_token_flow.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace {
std::vector<std::string> CreateTestScopes() {
  std::vector<std::string> scopes;
  scopes.push_back("scope1");
  scopes.push_back("scope2");
  return scopes;
}
}

class MockDelegate : public OAuth2MintTokenFlow::Delegate {
 public:
  MockDelegate() {}
  ~MockDelegate() {}

  MOCK_METHOD1(OnMintTokenSuccess, void(const std::string& access_token));
  MOCK_METHOD1(OnMintTokenFailure,
               void(const GoogleServiceAuthError& error));
};

class MockOAuth2AccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  MockOAuth2AccessTokenFetcher(OAuth2AccessTokenConsumer* consumer,
                               net::URLRequestContextGetter* getter)
      : OAuth2AccessTokenFetcher(consumer, getter) {}
  ~MockOAuth2AccessTokenFetcher() {}

  MOCK_METHOD4(Start,
               void (const std::string& client_id,
                     const std::string& client_secret,
                     const std::string& refresh_token,
                     const std::vector<std::string>& scopes));
};

class MockOAuth2MintTokenFetcher : public OAuth2MintTokenFetcher {
 public:
   MockOAuth2MintTokenFetcher(OAuth2MintTokenConsumer* consumer,
                              net::URLRequestContextGetter* getter)
      : OAuth2MintTokenFetcher(consumer, getter, "test") {}
  ~MockOAuth2MintTokenFetcher() {}

  MOCK_METHOD4(Start,
               void (const std::string& oauth_login_access_token,
                     const std::string& client_id,
                     const std::vector<std::string>& scopes,
                     const std::string& origin));
};

class MockOAuth2MintTokenFlow : public OAuth2MintTokenFlow {
 public:
  explicit MockOAuth2MintTokenFlow(MockDelegate* delegate)
      : OAuth2MintTokenFlow(NULL, delegate) {}
  ~MockOAuth2MintTokenFlow() {}

  MOCK_METHOD0(CreateAccessTokenFetcher, OAuth2AccessTokenFetcher*());
  MOCK_METHOD0(CreateMintTokenFetcher, OAuth2MintTokenFetcher*());
};

class OAuth2MintTokenFlowTest : public testing::Test {
 public:
  OAuth2MintTokenFlowTest() {
    flow_.reset(new MockOAuth2MintTokenFlow(&delegate_));
    access_token_fetcher_.reset(new MockOAuth2AccessTokenFetcher(
        flow_.get(), NULL));
    mint_token_fetcher_.reset(new MockOAuth2MintTokenFetcher(
        flow_.get(), NULL));
  }

  virtual ~OAuth2MintTokenFlowTest() { }

 protected:
  void SetAccessTokenFetcherFailure(const std::string& rt,
                                    const GoogleServiceAuthError& error) {
    EXPECT_CALL(*access_token_fetcher_,
        Start(GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
              GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
              rt, std::vector<std::string>()))
        .Times(1);
    EXPECT_CALL(*flow_, CreateAccessTokenFetcher())
        .WillOnce(Return(access_token_fetcher_.release()));
    EXPECT_CALL(delegate_, OnMintTokenFailure(error))
        .Times(1);
  }

  void SetAccessTokenFetcherSuccess(const std::string& rt) {
    EXPECT_CALL(*access_token_fetcher_,
        Start(GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
              GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
              rt, std::vector<std::string>()))
        .Times(1);
    EXPECT_CALL(*flow_, CreateAccessTokenFetcher())
        .WillOnce(Return(access_token_fetcher_.release()));
  }

  void SetMintTokenFetcherFailure(const std::string& rt,
                                  const std::string& ext_id,
                                  const std::string& client_id,
                                  const std::vector<std::string>& scopes,
                                  const GoogleServiceAuthError& error) {
    EXPECT_CALL(*mint_token_fetcher_,
        Start(rt, client_id, scopes, ext_id))
        .Times(1);
    EXPECT_CALL(*flow_, CreateMintTokenFetcher())
        .WillOnce(Return(mint_token_fetcher_.release()));
    EXPECT_CALL(delegate_, OnMintTokenFailure(error))
        .Times(1);
  }

  void SetMintTokenFetcherSuccess(const std::string& rt,
                                  const std::string& ext_id,
                                  const std::string& client_id,
                                  const std::vector<std::string>& scopes,
                                  const std::string& at) {
    EXPECT_CALL(*mint_token_fetcher_,
        Start(rt, client_id, scopes, ext_id))
        .Times(1);
    EXPECT_CALL(*flow_, CreateMintTokenFetcher())
        .WillOnce(Return(mint_token_fetcher_.release()));
    EXPECT_CALL(delegate_, OnMintTokenSuccess(at))
        .Times(1);
  }

  scoped_ptr<MockOAuth2MintTokenFlow> flow_;
  scoped_ptr<MockOAuth2AccessTokenFetcher> access_token_fetcher_;
  scoped_ptr<MockOAuth2MintTokenFetcher> mint_token_fetcher_;
  MockDelegate delegate_;
};

TEST_F(OAuth2MintTokenFlowTest, LoginAccessTokenFailure) {
  std::string rt = "refresh_token";
  std::string ext_id = "ext1";
  std::string client_id = "client1";
  std::vector<std::string> scopes(CreateTestScopes());
  std::string at = "access_token";
  GoogleServiceAuthError err(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);

  SetAccessTokenFetcherFailure(rt, err);

  flow_->Start(rt, ext_id, client_id, scopes);
  flow_->OnGetTokenFailure(err);
}

TEST_F(OAuth2MintTokenFlowTest, MintAccessTokenFailure) {
  std::string rt = "refresh_token";
  std::string ext_id = "ext1";
  std::string client_id = "client1";
  std::vector<std::string> scopes(CreateTestScopes());
  std::string at = "access_token";
  GoogleServiceAuthError err(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);

  SetAccessTokenFetcherSuccess(rt);
  SetMintTokenFetcherFailure(at, ext_id, client_id, scopes, err);

  flow_->Start(rt, ext_id, client_id, scopes);
  flow_->OnGetTokenSuccess(at);
  flow_->OnMintTokenFailure(err);
}

TEST_F(OAuth2MintTokenFlowTest, Success) {
  std::string rt = "refresh_token";
  std::string ext_id = "ext1";
  std::string client_id = "client1";
  std::vector<std::string> scopes(CreateTestScopes());
  std::string at = "access_token";
  std::string result = "app_access_token";

  SetAccessTokenFetcherSuccess(rt);
  SetMintTokenFetcherSuccess(at, ext_id, client_id, scopes, result);

  flow_->Start(rt, ext_id, client_id, scopes);
  flow_->OnGetTokenSuccess(at);
  flow_->OnMintTokenSuccess(result);
}
