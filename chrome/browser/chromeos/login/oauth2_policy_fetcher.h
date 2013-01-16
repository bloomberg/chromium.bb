// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH2_POLICY_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH2_POLICY_FETCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "net/url_request/url_request_context_getter.h"

class GaiaAuthFetcher;
class OAuth2AccessTokenFetcher;

namespace chromeos {

// Fetches the OAuth2 token for the device management service. Since Profile
// creation might be blocking on a user policy fetch, this fetcher must always
// send a (possibly empty) token to the BrowserPolicyConnector, which will then
// let the policy subsystem proceed and resume Profile creation.
// Sending the token even when no Profile is pending is also OK.
class OAuth2PolicyFetcher : public base::SupportsWeakPtr<OAuth2PolicyFetcher>,
                            public GaiaAuthConsumer,
                            public OAuth2AccessTokenConsumer {
 public:
  // Fetches the device management service's OAuth2 token using
  // |oauth2_tokens.refresh_token|.
  OAuth2PolicyFetcher(net::URLRequestContextGetter* system_context_getter,
                      const std::string& oauth2_refresh_token);
  // Fetches the device management service's oauth2 token, after also retrieving
  // the OAuth2 refresh tokens.
  OAuth2PolicyFetcher(
      net::URLRequestContextGetter* auth_context_getter,
      net::URLRequestContextGetter* system_context_getter);

  virtual ~OAuth2PolicyFetcher();

  // Starts process of minting device management service OAuth2 access token.
  void Start();

  // Returns OAuth2 tokens fetched through an authenticated cookie jar.
  const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens() const {
    return oauth2_tokens_;
  }

  // True if we have OAuth2 tokens that were fetched through an authenticated
  // cookie jar.
  bool has_oauth2_tokens() const {
    return !oauth2_tokens_.refresh_token.empty();
  }

  // Returns true if we have previously attempted to fetch tokens with this
  // class and failed.
  bool failed() const {
    return failed_;
  }

 private:
  // GaiaAuthConsumer overrides.
  virtual void OnClientOAuthSuccess(
      const GaiaAuthConsumer::ClientOAuthResult& oauth_tokens) OVERRIDE;
  virtual void OnClientOAuthFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  // OAuth2AccessTokenConsumer overrides.
  virtual void OnGetTokenSuccess(const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // Starts fetching OAuth2 refresh token.
  void StartFetchingRefreshToken();
  // Starts fetching OAuth2 access token for the device management service.
  void StartFetchingAccessToken();

  void SetPolicyToken(const std::string& token);
  // Decides how to proceed on GAIA |error|. If the error looks temporary,
  // retries |task| until max retry count is reached.
  // If retry count runs out, or error condition is unrecoverable, it calls
  // Delegate::OnOAuth2TokenFetchFailed().
  void RetryOnError(const GoogleServiceAuthError& error,
                    const base::Closure& task);

  scoped_refptr<net::URLRequestContextGetter> auth_context_getter_;
  scoped_refptr<net::URLRequestContextGetter> system_context_getter_;
  scoped_ptr<GaiaAuthFetcher> refresh_token_fetcher_;
  scoped_ptr<OAuth2AccessTokenFetcher> access_token_fetcher_;
  std::string policy_token_;
  GaiaAuthConsumer::ClientOAuthResult oauth2_tokens_;
  // OAuth2 refresh token. Could come either from the outside or through
  // refresh token fetching flow within this class.
  std::string oauth2_refresh_token_;
  // The retry counter. Increment this only when failure happened.
  int retry_count_;
  // True if we have already failed to fetch the policy.
  bool failed_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2PolicyFetcher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH2_POLICY_FETCHER_H_
