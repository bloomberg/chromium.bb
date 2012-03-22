// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_GAIA_OAUTH2_MINT_TOKEN_FLOW_H_
#define CHROME_COMMON_NET_GAIA_OAUTH2_MINT_TOKEN_FLOW_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/net/gaia/oauth2_access_token_consumer.h"
#include "chrome/common/net/gaia/oauth2_access_token_fetcher.h"
#include "chrome/common/net/gaia/oauth2_mint_token_consumer.h"
#include "chrome/common/net/gaia/oauth2_mint_token_fetcher.h"

class GoogleServiceAuthError;
class OAuth2MintTokenFlowTest;

namespace net {
class URLRequestContextGetter;
}

// This class implements the OAuth2 flow to Google to mint an OAuth2
// token for the given client and the given set of scopes from the
// OAuthLogin scoped "master" OAuth2 token for the user logged in to
// Chrome.
class OAuth2MintTokenFlow
    : public OAuth2AccessTokenConsumer,
      public OAuth2MintTokenConsumer {
 public:
  class Delegate {
   public:
    virtual void OnMintTokenSuccess(const std::string& access_token) { }
    virtual void OnMintTokenFailure(const GoogleServiceAuthError& error) { }
  };

  // An interceptor for tests.
  class InterceptorForTests {
   public:
    // Returns true if the success callback should be called and false for
    // failures.
    virtual bool DoIntercept(const OAuth2MintTokenFlow* flow,
                             std::string* access_token,
                             GoogleServiceAuthError* error) = 0;
  };
  static void SetInterceptorForTests(InterceptorForTests* interceptor);

  OAuth2MintTokenFlow(net::URLRequestContextGetter* context,
                      Delegate* delegate);
  virtual ~OAuth2MintTokenFlow();

  // Start the process to mint a token.
  void Start(const std::string& login_refresh_token,
             const std::string& extension_id,
             const std::string& client_id,
             const std::vector<std::string>& scopes);

  // OAuth2AccessTokenConsumer implementation.
  virtual void OnGetTokenSuccess(const std::string& access_token) OVERRIDE;
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;
  // OAuth2MintTokenConsumer implementation.
  virtual void OnMintTokenSuccess(const std::string& access_token) OVERRIDE;
  virtual void OnMintTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // Getters for various members.
  const std::string& extension_id() const { return extension_id_; }
  const std::string& client_id() const { return client_id_; }

 protected:
  // Helper to create an instance of access token fetcher.
  // Caller owns the returned instance.
  virtual OAuth2AccessTokenFetcher* CreateAccessTokenFetcher();

  // Helper to create an instance of mint token fetcher.
  // Caller owns the returned instance.
  virtual OAuth2MintTokenFetcher* CreateMintTokenFetcher();

 private:
  // The steps this class performs are:
  // 1. Create a login scoped access token from login scoped refresh token.
  // 2. Use login scoped access token to call the API to mint an access token
  //    for the app.
  enum State {
    INITIAL,
    FETCH_LOGIN_ACCESS_TOKEN_STARTED,
    FETCH_LOGIN_ACCESS_TOKEN_DONE,
    MINT_ACCESS_TOKEN_STARTED,
    MINT_ACCESS_TOKEN_DONE,
    ERROR_STATE
  };

  enum SetupError {
    NONE,
    AUTH_ERROR,
    INTERNAL_ERROR,
    USER_CANCELLED,

    // This is used for histograms, and should always be the last value.
    SETUP_ERROR_BOUNDARY
  };

  friend class OAuth2MintTokenFlowTest;

  // Creates an instance of URLFetcher that does not send or save cookies.
  // The URLFether's method will be GET if body is empty, POST otherwise.
  // Caller owns the returned instance.
  content::URLFetcher* CreateURLFetcher(
      const GURL& url, const std::string& body, const std::string& auth_token);
  void BeginGetLoginAccessToken();
  void EndGetLoginAccessToken(const GoogleServiceAuthError* error);
  void BeginMintAccessToken();
  void EndMintAccessToken(const GoogleServiceAuthError* error);

  void ReportSuccess();
  void ReportFailure(const GoogleServiceAuthError& error);

  static std::string GetErrorString(SetupError error);

  net::URLRequestContextGetter* context_;
  Delegate* delegate_;
  State state_;

  std::string login_refresh_token_;
  std::string extension_id_;
  std::string client_id_;
  std::vector<std::string> scopes_;

  scoped_ptr<OAuth2AccessTokenFetcher> oauth2_access_token_fetcher_;
  scoped_ptr<OAuth2MintTokenFetcher> oauth2_mint_token_fetcher_;
  std::string login_access_token_;
  std::string app_access_token_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2MintTokenFlow);
};

#endif  // CHROME_COMMON_NET_GAIA_OAUTH2_MINT_TOKEN_FLOW_H_
