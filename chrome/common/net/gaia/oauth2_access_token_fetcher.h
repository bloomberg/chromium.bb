// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_H_
#define CHROME_COMMON_NET_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_H_
#pragma once

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/net/gaia/oauth2_access_token_consumer.h"
#include "content/public/common/url_fetcher.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

class OAuth2AccessTokenFetcherTest;

namespace net {
class URLRequestContextGetter;
class URLRequestStatus;
}

// Abstracts the details to get OAuth2 access token token from
// OAuth2 refresh token.
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
class OAuth2AccessTokenFetcher : public content::URLFetcherDelegate {
 public:
  OAuth2AccessTokenFetcher(OAuth2AccessTokenConsumer* consumer,
                           net::URLRequestContextGetter* getter,
                           const std::string& source);
  virtual ~OAuth2AccessTokenFetcher();

  void Start(const std::string& refresh_token);

  void CancelRequest();

  // Implementation of content::URLFetcherDelegate
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

 private:
  enum State {
    INITIAL,
    GET_ACCESS_TOKEN_STARTED,
    GET_ACCESS_TOKEN_DONE,
    ERROR_STATE,
  };

  // Helper methods for the flow.
  void StartGetAccessToken();
  void EndGetAccessToken(const content::URLFetcher* source);

  // Helper mehtods for reporting back results.
  void OnGetTokenSuccess(const std::string& access_token);
  void OnGetTokenFailure(GoogleServiceAuthError error);

  // Other helpers.
  static GURL MakeGetAccessTokenUrl();
  static std::string MakeGetAccessTokenBody(const std::string& refresh_token);
  static bool ParseGetAccessTokenResponse(const content::URLFetcher* source,
                                          std::string* access_token);

  // State that is set during construction.
  OAuth2AccessTokenConsumer* const consumer_;
  net::URLRequestContextGetter* const getter_;
  std::string source_;
  State state_;

  // While a fetch is in progress.
  scoped_ptr<content::URLFetcher> fetcher_;
  std::string refresh_token_;

  friend class OAuth2AccessTokenFetcherTest;
  FRIEND_TEST_ALL_PREFIXES(OAuth2AccessTokenFetcherTest,
                           ParseGetAccessTokenResponse);

  DISALLOW_COPY_AND_ASSIGN(OAuth2AccessTokenFetcher);
};

#endif  // CHROME_COMMON_NET_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_H_
