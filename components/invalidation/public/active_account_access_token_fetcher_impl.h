// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_ACTIVE_ACCOUNT_ACCESS_TOKEN_FETCHER_IMPL_H_
#define COMPONENTS_INVALIDATION_PUBLIC_ACTIVE_ACCOUNT_ACCESS_TOKEN_FETCHER_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/invalidation/public/identity_provider.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace invalidation {

// An implementation of ActiveAccountAccessTokenFetcher that is backed by
// OAuth2TokenService.
// TODO(809452): Once ProfileIdentityProvider is converted to talk to
// IdentityManager, this helper class should either be hidden inside
// DeviceIdentityProvider or moved to live next to it.
class ActiveAccountAccessTokenFetcherImpl
    : public ActiveAccountAccessTokenFetcher,
      OAuth2TokenService::Consumer {
 public:
  ActiveAccountAccessTokenFetcherImpl(
      const std::string& active_account_id,
      const std::string& oauth_consumer_name,
      OAuth2TokenService* token_service,
      const OAuth2TokenService::ScopeSet& scopes,
      ActiveAccountAccessTokenCallback callback);
  ~ActiveAccountAccessTokenFetcherImpl() override;

 private:
  // OAuth2TokenService::Consumer implementation.
  void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // Invokes |callback_| with (|access_token|, |error|).
  void HandleTokenRequestCompletion(const OAuth2TokenService::Request* request,
                                    const GoogleServiceAuthError& error,
                                    const std::string& access_token);

  ActiveAccountAccessTokenCallback callback_;
  std::unique_ptr<OAuth2TokenService::Request> access_token_request_;

  DISALLOW_COPY_AND_ASSIGN(ActiveAccountAccessTokenFetcherImpl);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_ACTIVE_ACCOUNT_ACCESS_TOKEN_FETCHER_IMPL_H_
