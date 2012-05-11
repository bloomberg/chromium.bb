// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_GAIA_OAUTH2_MINT_TOKEN_FETCHER_H_
#define CHROME_COMMON_NET_GAIA_OAUTH2_MINT_TOKEN_FETCHER_H_
#pragma once

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/net/gaia/oauth2_mint_token_consumer.h"
#include "content/public/common/url_fetcher.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

class OAuth2MintTokenFetcherTest;

namespace net {
class URLRequestContextGetter;
class URLRequestStatus;
}

// Abstracts the details to mint new OAuth2 tokens from OAuth2 login scoped
// token.
//
// This class should be used on a single thread, but it can be whichever thread
// that you like.
// Also, do not reuse the same instance. Once Start() is called, the instance
// should not be reused.
//
// Usage:
// * Create an instance with a consumer.
// * Call Start()
// * The consumer passed in the constructor will be called on the same
//   thread Start was called with the results.
//
// This class can handle one request at a time. To parallelize requests,
// create multiple instances.
class OAuth2MintTokenFetcher : public content::URLFetcherDelegate {
 public:
  OAuth2MintTokenFetcher(OAuth2MintTokenConsumer* consumer,
                         net::URLRequestContextGetter* getter,
                         const std::string& source);
  virtual ~OAuth2MintTokenFetcher();

  // Start the flow.
  virtual void Start(const std::string& oauth_login_access_token,
                     const std::string& client_id,
                     const std::vector<std::string>& scopes,
                     const std::string& origin);

  void CancelRequest();

  // Implementation of content::URLFetcherDelegate
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  enum State {
    INITIAL,
    MINT_TOKEN_STARTED,
    MINT_TOKEN_DONE,
    ERROR_STATE,
  };

  // Helper methods for the flow.
  void StartMintToken();
  void EndMintToken(const net::URLFetcher* source);

  // Helper methods for reporting back results.
  void OnMintTokenSuccess(const std::string& access_token);
  void OnMintTokenFailure(const GoogleServiceAuthError& error);

  // Other helpers.
  static GURL MakeMintTokenUrl();
  static std::string MakeMintTokenHeader(const std::string& access_token);
  static std::string MakeMintTokenBody(const std::string& client_id,
                                       const std::vector<std::string>& scopes,
                                       const std::string& origin);
  static bool ParseMintTokenResponse(const net::URLFetcher* source,
                                     std::string* access_token);

  // State that is set during construction.
  OAuth2MintTokenConsumer* const consumer_;
  net::URLRequestContextGetter* const getter_;
  std::string source_;
  State state_;

  // While a fetch is in progress.
  scoped_ptr<content::URLFetcher> fetcher_;
  std::string oauth_login_access_token_;
  std::string client_id_;
  std::vector<std::string> scopes_;
  std::string origin_;

  friend class OAuth2MintTokenFetcherTest;
  FRIEND_TEST_ALL_PREFIXES(OAuth2MintTokenFetcherTest,
                           ParseMintTokenResponse);

  DISALLOW_COPY_AND_ASSIGN(OAuth2MintTokenFetcher);
};

#endif  // CHROME_COMMON_NET_GAIA_OAUTH2_MINT_TOKEN_FETCHER_H_
