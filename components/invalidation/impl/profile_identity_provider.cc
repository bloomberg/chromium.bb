// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/profile_identity_provider.h"

#include "components/invalidation/public/active_account_access_token_fetcher_impl.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"

namespace invalidation {

ProfileIdentityProvider::ProfileIdentityProvider(
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service)
    : signin_manager_(signin_manager), token_service_(token_service) {
  // TODO(blundell): Can |token_service_| ever actually be non-null?
  if (token_service_)
    token_service_->AddObserver(this);
  signin_manager_->AddObserver(this);
}

ProfileIdentityProvider::~ProfileIdentityProvider() {
  // TODO(blundell): Can |token_service_| ever actually be non-null?
  if (token_service_)
    token_service_->RemoveObserver(this);

  // In unittests |signin_manager_| is allowed to be null.
  // TODO(809452): Eliminate this short-circuit when this class is converted to
  // take in IdentityManager, at which point the tests can use
  // IdentityTestEnvironment.
  if (signin_manager_)
    signin_manager_->RemoveObserver(this);
}

std::string ProfileIdentityProvider::GetActiveAccountId() {
  // In unittests |signin_manager_| is allowed to be null.
  // TODO(809452): Eliminate this short-circuit when this class is converted to
  // take in IdentityManager, at which point the tests can use
  // IdentityTestEnvironment.
  if (!signin_manager_)
    return std::string();

  return signin_manager_->GetAuthenticatedAccountId();
}

bool ProfileIdentityProvider::IsActiveAccountAvailable() {
  if (GetActiveAccountId().empty() || !token_service_ ||
      !token_service_->RefreshTokenIsAvailable(GetActiveAccountId()))
    return false;

  return true;
}

std::unique_ptr<ActiveAccountAccessTokenFetcher>
ProfileIdentityProvider::FetchAccessToken(
    const std::string& oauth_consumer_name,
    const OAuth2TokenService::ScopeSet& scopes,
    ActiveAccountAccessTokenCallback callback) {
  return std::make_unique<ActiveAccountAccessTokenFetcherImpl>(
      GetActiveAccountId(), oauth_consumer_name, token_service_, scopes,
      std::move(callback));
}

void ProfileIdentityProvider::InvalidateAccessToken(
    const OAuth2TokenService::ScopeSet& scopes,
    const std::string& access_token) {
  token_service_->InvalidateAccessToken(GetActiveAccountId(), scopes,
                                        access_token);
}

void ProfileIdentityProvider::GoogleSigninSucceeded(
    const std::string& account_id,
    const std::string& username) {
  FireOnActiveAccountLogin();
}

void ProfileIdentityProvider::GoogleSignedOut(const std::string& account_id,
                                              const std::string& username) {
  FireOnActiveAccountLogout();
}

void ProfileIdentityProvider::OnRefreshTokenAvailable(
    const std::string& account_id) {
  ProcessRefreshTokenUpdateForAccount(account_id);
}

void ProfileIdentityProvider::OnRefreshTokenRevoked(
    const std::string& account_id) {
  ProcessRefreshTokenRemovalForAccount(account_id);
}

}  // namespace invalidation
