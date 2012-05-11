// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_GAIA_OAUTH2_API_CALL_FLOW_H_
#define CHROME_COMMON_NET_GAIA_OAUTH2_API_CALL_FLOW_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/common/net/gaia/oauth2_access_token_consumer.h"
#include "chrome/common/net/gaia/oauth2_access_token_fetcher.h"
#include "chrome/common/net/gaia/oauth2_mint_token_consumer.h"
#include "chrome/common/net/gaia/oauth2_mint_token_fetcher.h"
#include "content/public/common/url_fetcher.h"
#include "content/public/common/url_fetcher_delegate.h"

class GoogleServiceAuthError;
class OAuth2MintTokenFlowTest;

namespace net {
class URLRequestContextGetter;
}

// Base class for all classes that implement a flow to call OAuth2
// enabled APIs.
//
// Given a refresh token, an access token, and a list of scopes an OAuth2
// enabled API is called in the following way:
// 1. Try the given access token to call the API.
// 2. If that does not work, use the refresh token and scopes to generate
//    a new access token.
// 3. Try the new access token to call the API.
//
// This class abstracts the basic steps and exposes template methods
// for sub-classes to implement for API specific details.
class OAuth2ApiCallFlow
    : public content::URLFetcherDelegate,
      public OAuth2AccessTokenConsumer {
 public:
  // Creates an instance that works with the given data.
  // Note that |access_token| can be empty. In that case, the flow will skip
  // the first step (of trying an existing access token).
  OAuth2ApiCallFlow(
      net::URLRequestContextGetter* context,
      const std::string& refresh_token,
      const std::string& access_token,
      const std::vector<std::string>& scopes);

  virtual ~OAuth2ApiCallFlow();

  // Start the flow.
  virtual void Start();

  // OAuth2AccessTokenFetcher implementation.
  virtual void OnGetTokenSuccess(const std::string& access_token) OVERRIDE;
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // content::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 protected:
  // Template methods for sub-classes.

  // Methods to help create HTTP request.
  virtual GURL CreateApiCallUrl() = 0;
  virtual std::string CreateApiCallBody() = 0;

  // Sub-classes can expose an appropriate observer interface by implementing
  // these template methods.
  // Called when the API call finished successfully.
  virtual void ProcessApiCallSuccess(const net::URLFetcher* source) = 0;
  // Called when the API call failed.
  virtual void ProcessApiCallFailure(const net::URLFetcher* source) = 0;
  // Called when a new access token is generated.
  virtual void ProcessNewAccessToken(const std::string& access_token) = 0;
  virtual void ProcessMintAccessTokenFailure(
      const GoogleServiceAuthError& error) = 0;

 private:
  enum State {
    INITIAL,
    API_CALL_STARTED,
    API_CALL_DONE,
    MINT_ACCESS_TOKEN_STARTED,
    MINT_ACCESS_TOKEN_DONE,
    ERROR_STATE
  };

  friend class OAuth2ApiCallFlowTest;
  FRIEND_TEST_ALL_PREFIXES(OAuth2ApiCallFlowTest, CreateURLFetcher);

  // Helper to create an instance of access token fetcher.
  // Caller owns the returned instance.
  // Note that this is virtual since it is mocked during unit testing.
  virtual OAuth2AccessTokenFetcher* CreateAccessTokenFetcher();

  // Creates an instance of URLFetcher that does not send or save cookies.
  // Template method CreateApiCallUrl is used to get the URL.
  // Template method CreateApiCallBody is used to get the body.
  // The URLFether's method will be GET if body is empty, POST otherwise.
  // Caller owns the returned instance.
  // Note that this is virtual since it is mocked during unit testing.
  virtual content::URLFetcher* CreateURLFetcher();

  // Helper methods to implement the state machine for the flow.
  void BeginApiCall();
  void EndApiCall(const net::URLFetcher* source);
  void BeginMintAccessToken();
  void EndMintAccessToken(const GoogleServiceAuthError* error);

  net::URLRequestContextGetter* context_;
  std::string refresh_token_;
  std::string access_token_;
  std::vector<std::string> scopes_;

  State state_;
  // Whether we have already tried minting an access token once.
  bool tried_mint_access_token_;

  scoped_ptr<content::URLFetcher> url_fetcher_;
  scoped_ptr<OAuth2AccessTokenFetcher> oauth2_access_token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2ApiCallFlow);
};

#endif  // CHROME_COMMON_NET_GAIA_OAUTH2_API_CALL_FLOW_H_
