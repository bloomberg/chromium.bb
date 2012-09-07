// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_REVOCATION_FETCHER_H_
#define GOOGLE_APIS_GAIA_OAUTH2_REVOCATION_FETCHER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "google_apis/gaia/oauth2_revocation_consumer.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher_delegate.h"

class OAuth2RevocationFetcherTest;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
class URLRequestStatus;
}

// Abstracts the details to perform OAuth2 grant revocation.
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
class OAuth2RevocationFetcher : public net::URLFetcherDelegate {
 public:
  OAuth2RevocationFetcher(OAuth2RevocationConsumer* consumer,
                          net::URLRequestContextGetter* getter);
  virtual ~OAuth2RevocationFetcher();

  // Starts the flow with the given parameters.
  // |access_token| should be an OAuth2 login scoped access token.
  void Start(const std::string& access_token,
             const std::string& client_id,
             const std::string& origin);

  void CancelRequest();

  // Implementation of net::URLFetcherDelegate
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  enum State {
    INITIAL,
    REVOCATION_STARTED,
    REVOCATION_DONE,
    ERROR_STATE,
  };

  // Helper methods for the flow.
  void StartRevocation();
  void EndRevocation(const net::URLFetcher* source);

  // Helper mehtods for reporting back results.
  void OnRevocationSuccess();
  void OnRevocationFailure(const GoogleServiceAuthError& error);

  // Other helpers.
  static GURL MakeRevocationUrl();
  static std::string MakeRevocationHeader(const std::string& access_token);
  static std::string MakeRevocationBody(const std::string& client_id,
                                        const std::string& origin);

  // State that is set during construction.
  OAuth2RevocationConsumer* const consumer_;
  net::URLRequestContextGetter* const getter_;
  State state_;

  // While a fetch is in progress.
  scoped_ptr<net::URLFetcher> fetcher_;
  std::string access_token_;
  std::string client_id_;
  std::string origin_;

  friend class OAuth2RevocationFetcherTest;

  DISALLOW_COPY_AND_ASSIGN(OAuth2RevocationFetcher);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_REVOCATION_FETCHER_H_
