// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_H_
#define GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher_delegate.h"

class OAuth2AccessTokenFetcherTest;

namespace base {
class Time;
}

namespace net {
class URLFetcher;
class URLRequestContextGetter;
class URLRequestStatus;
}

// Abstracts the details to get OAuth2 access token token from
// OAuth2 refresh token.
// See "Using the Refresh Token" section in:
// http://code.google.com/apis/accounts/docs/OAuth2WebServer.html
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
class OAuth2AccessTokenFetcher : public net::URLFetcherDelegate {
 public:
  OAuth2AccessTokenFetcher(OAuth2AccessTokenConsumer* consumer,
                           net::URLRequestContextGetter* getter);
  virtual ~OAuth2AccessTokenFetcher();

  // Starts the flow with the given parameters.
  // |scopes| can be empty. If it is empty then the access token will have the
  // same scope as the refresh token. If not empty, then access token will have
  // the scopes specified. In this case, the access token will successfully be
  // generated only if refresh token has login scope of a list of scopes that is
  // a super-set of the specified scopes.
  virtual void Start(const std::string& client_id,
                     const std::string& client_secret,
                     const std::string& refresh_token,
                     const std::vector<std::string>& scopes);

  void CancelRequest();

  // Implementation of net::URLFetcherDelegate
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  enum State {
    INITIAL,
    GET_ACCESS_TOKEN_STARTED,
    GET_ACCESS_TOKEN_DONE,
    ERROR_STATE,
  };

  // Helper methods for the flow.
  void StartGetAccessToken();
  void EndGetAccessToken(const net::URLFetcher* source);

  // Helper mehtods for reporting back results.
  void OnGetTokenSuccess(const std::string& access_token,
                         const base::Time& expiration_time);
  void OnGetTokenFailure(const GoogleServiceAuthError& error);

  // Other helpers.
  static GURL MakeGetAccessTokenUrl();
  static std::string MakeGetAccessTokenBody(
      const std::string& client_id,
      const std::string& client_secret,
      const std::string& refresh_token,
      const std::vector<std::string>& scopes);
  static bool ParseGetAccessTokenResponse(const net::URLFetcher* source,
                                          std::string* access_token,
                                          int* expires_in);

  // State that is set during construction.
  OAuth2AccessTokenConsumer* const consumer_;
  net::URLRequestContextGetter* const getter_;
  State state_;

  // While a fetch is in progress.
  scoped_ptr<net::URLFetcher> fetcher_;
  std::string client_id_;
  std::string client_secret_;
  std::string refresh_token_;
  std::vector<std::string> scopes_;

  friend class OAuth2AccessTokenFetcherTest;
  FRIEND_TEST_ALL_PREFIXES(OAuth2AccessTokenFetcherTest,
                           ParseGetAccessTokenResponse);
  FRIEND_TEST_ALL_PREFIXES(OAuth2AccessTokenFetcherTest,
                           MakeGetAccessTokenBody);

  DISALLOW_COPY_AND_ASSIGN(OAuth2AccessTokenFetcher);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_H_
