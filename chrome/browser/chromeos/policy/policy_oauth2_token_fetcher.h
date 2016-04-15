// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_POLICY_OAUTH2_TOKEN_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_POLICY_OAUTH2_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"

class GaiaAuthFetcher;
class OAuth2AccessTokenFetcher;

namespace net {
class URLRequestContextGetter;
}

namespace policy {

// Fetches the OAuth2 token for the device management service. Since Profile
// creation might be blocking on a user policy fetch, this fetcher must always
// send a (possibly empty) token to the callback, which will then let the policy
// subsystem proceed and resume Profile creation. Sending the token even when no
// Profile is pending is also OK.
class PolicyOAuth2TokenFetcher
    : public base::SupportsWeakPtr<PolicyOAuth2TokenFetcher>,
      public GaiaAuthConsumer,
      public OAuth2AccessTokenConsumer {
 public:
  typedef base::Callback<void(const std::string&,
                              const GoogleServiceAuthError&)> TokenCallback;

  PolicyOAuth2TokenFetcher();
  ~PolicyOAuth2TokenFetcher() override;

  // Fetches the device management service's oauth2 token. This may be fetched
  // via signin context, auth code, or oauth2 refresh token.
  void StartWithSigninContext(
      net::URLRequestContextGetter* auth_context_getter,
      net::URLRequestContextGetter* system_context_getter,
      const TokenCallback& callback);
  void StartWithAuthCode(const std::string& auth_code,
                         net::URLRequestContextGetter* system_context_getter,
                         const TokenCallback& callback);
  void StartWithRefreshToken(
      const std::string& oauth2_refresh_token,
      net::URLRequestContextGetter* system_context_getter,
      const TokenCallback& callback);

  // Returns true if we have previously attempted to fetch tokens with this
  // class and failed.
  bool failed() const {
    return failed_;
  }

  const std::string& oauth2_refresh_token() const {
    return oauth2_refresh_token_;
  }
  const std::string& oauth2_access_token() const {
    return oauth2_access_token_;
  }

 private:
  // GaiaAuthConsumer overrides.
  void OnClientOAuthSuccess(
      const GaiaAuthConsumer::ClientOAuthResult& oauth_tokens) override;
  void OnClientOAuthFailure(const GoogleServiceAuthError& error) override;

  // OAuth2AccessTokenConsumer overrides.
  void OnGetTokenSuccess(const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const GoogleServiceAuthError& error) override;

  // Starts fetching OAuth2 refresh token.
  void StartFetchingRefreshToken();

  // Starts fetching OAuth2 access token for the device management service.
  void StartFetchingAccessToken();

  // Decides how to proceed on GAIA |error|. If the error looks temporary,
  // retries |task| until max retry count is reached.
  // If retry count runs out, or error condition is unrecoverable, it calls
  // Delegate::OnOAuth2TokenFetchFailed().
  void RetryOnError(const GoogleServiceAuthError& error,
                    const base::Closure& task);

  // Passes |token| and |error| to the |callback_|.
  void ForwardPolicyToken(const std::string& token,
                          const GoogleServiceAuthError& error);

  // Auth code which is used to retreive a refresh token.
  std::string auth_code_;

  scoped_refptr<net::URLRequestContextGetter> auth_context_getter_;
  scoped_refptr<net::URLRequestContextGetter> system_context_getter_;
  std::unique_ptr<GaiaAuthFetcher> refresh_token_fetcher_;
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher_;

  // OAuth2 refresh token. Could come either from the outside or through
  // refresh token fetching flow within this class.
  std::string oauth2_refresh_token_;

  // OAuth2 access token.
  std::string oauth2_access_token_;

  // The retry counter. Increment this only when failure happened.
  int retry_count_ = 0;

  // True if we have already failed to fetch the policy.
  bool failed_ = false;

  // The callback to invoke when done.
  TokenCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PolicyOAuth2TokenFetcher);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_POLICY_OAUTH2_TOKEN_FETCHER_H_
