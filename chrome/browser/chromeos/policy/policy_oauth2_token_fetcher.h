// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_POLICY_OAUTH2_TOKEN_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_POLICY_OAUTH2_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

// Fetches the OAuth2 token for the device management service. Since Profile
// creation might be blocking on a user policy fetch, this fetcher must always
// send a (possibly empty) token to the callback, which will then let the policy
// subsystem proceed and resume Profile creation. Sending the token even when no
// Profile is pending is also OK.
class PolicyOAuth2TokenFetcher {
 public:
  // Allocates a PolicyOAuth2TokenFetcher instance.
  static std::unique_ptr<PolicyOAuth2TokenFetcher> CreateInstance();

  // Makes CreateInstance() return a fake token fetcher that does not make
  // network calls so tests can avoid a dependency on GAIA.
  static void UseFakeTokensForTesting();

  using TokenCallback =
      base::Callback<void(const std::string&, const GoogleServiceAuthError&)>;

  PolicyOAuth2TokenFetcher();
  virtual ~PolicyOAuth2TokenFetcher();

  // Fetches the device management service's oauth2 token. This may be fetched
  // via signin context, auth code, or oauth2 refresh token.
  virtual void StartWithAuthCode(
      const std::string& auth_code,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory,
      const TokenCallback& callback) = 0;
  virtual void StartWithRefreshToken(
      const std::string& oauth2_refresh_token,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory,
      const TokenCallback& callback) = 0;

  // Returns true if we have previously attempted to fetch tokens with this
  // class and failed.
  virtual bool Failed() const = 0;
  virtual const std::string& OAuth2RefreshToken() const = 0;
  virtual const std::string& OAuth2AccessToken() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyOAuth2TokenFetcher);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_POLICY_OAUTH2_TOKEN_FETCHER_H_
