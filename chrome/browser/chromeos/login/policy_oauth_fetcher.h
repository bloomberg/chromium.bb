// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_POLICY_OAUTH_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_POLICY_OAUTH_FETCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/net/gaia/gaia_oauth_consumer.h"
#include "chrome/browser/net/gaia/gaia_oauth_fetcher.h"

namespace net {
class URLRequestContextGetter;
}

namespace chromeos {

// Fetches the oauth token for the device management service. Since Profile
// creation might be blocking on a user policy fetch, this fetcher must always
// send a (possibly empty) token to the BrowserPolicyConnector, which will then
// let the policy subsystem proceed and resume Profile creation.
// Sending the token even when no Profile is pending is also OK.
class PolicyOAuthFetcher : public GaiaOAuthConsumer {
 public:
  // Fetches the device management service's oauth token using |oauth1_token|
  // and |oauth1_secret| as access tokens.
  PolicyOAuthFetcher(net::URLRequestContextGetter* context_getter,
                     const std::string& oauth1_token,
                     const std::string& oauth1_secret);
  // Fetches the device management service's oauth token, after also retrieving
  // the access tokens.
  explicit PolicyOAuthFetcher(net::URLRequestContextGetter* context_getter);
  virtual ~PolicyOAuthFetcher();

  void Start();

  const std::string& oauth1_token() const { return oauth1_token_; }
  const std::string& oauth1_secret() const { return oauth1_secret_; }
  bool failed() const {
    return !oauth_fetcher_.HasPendingFetch() && policy_token_.empty();
  }

 private:
  // GaiaOAuthConsumer overrides.
  virtual void OnGetOAuthTokenSuccess(const std::string& oauth_token) OVERRIDE;
  virtual void OnGetOAuthTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnOAuthGetAccessTokenSuccess(
      const std::string& token,
      const std::string& secret) OVERRIDE;
  virtual void OnOAuthGetAccessTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnOAuthWrapBridgeSuccess(
      const std::string& service_name,
      const std::string& token,
      const std::string& expires_in) OVERRIDE;
  virtual void OnOAuthWrapBridgeFailure(
      const std::string& service_name,
      const GoogleServiceAuthError& error) OVERRIDE;

  void SetPolicyToken(const std::string& token);

  GaiaOAuthFetcher oauth_fetcher_;
  std::string oauth1_token_;
  std::string oauth1_secret_;
  std::string policy_token_;

  DISALLOW_COPY_AND_ASSIGN(PolicyOAuthFetcher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_POLICY_OAUTH_FETCHER_H_
