// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_OBFUSCATED_GAIA_ID_FETCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_OBFUSCATED_GAIA_ID_FETCHER_H_

#include <string>

#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "google_apis/gaia/oauth2_token_service.h"

class GoogleServiceAuthError;

namespace content {
class URLFetcher;
}

namespace net {
class URLRequestContextGetter;
}

namespace extensions {

// Fetches obfuscated Gaia ID of the Google Account that is logged in to Chrome.
// This starts an asynchronous operation which reports success to the delegate.
// Call "Start()" to start the async fetch.
class ObfuscatedGaiaIdFetcher : public OAuth2ApiCallFlow {
 public:
  // Delegate interface that is called when obfuscated Gaia id fetch
  // succeeds or fails.
  class Delegate {
   public:
    virtual void OnObfuscatedGaiaIdFetchSuccess(
        const std::string& obfuscated_id) {}
    virtual void OnObfuscatedGaiaIdFetchFailure(
        const GoogleServiceAuthError& error) {}
    virtual ~Delegate() {}
  };

  // TODO(petewil): Someday let's make a profile keyed service to cache
  // the Gaia ID.
  explicit ObfuscatedGaiaIdFetcher(Delegate* delegate);
  ~ObfuscatedGaiaIdFetcher() override;

  static OAuth2TokenService::ScopeSet GetScopes();

 protected:
  // OAuth2ApiCallFlow implementation
  GURL CreateApiCallUrl() override;
  std::string CreateApiCallBody() override;
  void ProcessApiCallSuccess(const net::URLFetcher* source) override;
  void ProcessApiCallFailure(const net::URLFetcher* source) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ObfuscatedGaiaIdFetcherTest, SetUp);
  FRIEND_TEST_ALL_PREFIXES(ObfuscatedGaiaIdFetcherTest, ParseResponse);
  FRIEND_TEST_ALL_PREFIXES(ObfuscatedGaiaIdFetcherTest, ProcessApiCallSuccess);
  FRIEND_TEST_ALL_PREFIXES(ObfuscatedGaiaIdFetcherTest, ProcessApiCallFailure);

  void ReportSuccess(const std::string& obfuscated_id);
  void ReportFailure(const GoogleServiceAuthError& error);

  // Get the obfuscated Gaia ID out of the response body.
  static bool ParseResponse(
      const std::string& data, std::string* obfuscated_id);

  // Unowned pointer to the delegate.  Normally the delegate owns
  // this fetcher class.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ObfuscatedGaiaIdFetcher);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PUSH_MESSAGING_OBFUSCATED_GAIA_ID_FETCHER_H_
