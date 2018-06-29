// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_identity_provider.h"

#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "components/invalidation/public/active_account_access_token_fetcher_impl.h"

namespace chromeos {

DeviceIdentityProvider::DeviceIdentityProvider(
    chromeos::DeviceOAuth2TokenService* token_service)
    : token_service_(token_service) {
  // TODO(blundell): Can |token_service_| ever actually be non-null?
  if (token_service_)
    token_service_->AddObserver(this);
}

DeviceIdentityProvider::~DeviceIdentityProvider() {
  // TODO(blundell): Can |token_service_| ever actually be non-null?
  if (token_service_)
    token_service_->RemoveObserver(this);
}

std::string DeviceIdentityProvider::GetActiveAccountId() {
  return token_service_->GetRobotAccountId();
}

bool DeviceIdentityProvider::IsActiveAccountAvailable() {
  if (GetActiveAccountId().empty() || !token_service_ ||
      !token_service_->RefreshTokenIsAvailable(GetActiveAccountId()))
    return false;

  return true;
}

std::unique_ptr<invalidation::ActiveAccountAccessTokenFetcher>
DeviceIdentityProvider::FetchAccessToken(
    const std::string& oauth_consumer_name,
    const OAuth2TokenService::ScopeSet& scopes,
    invalidation::ActiveAccountAccessTokenCallback callback) {
  return std::make_unique<invalidation::ActiveAccountAccessTokenFetcherImpl>(
      GetActiveAccountId(), oauth_consumer_name, token_service_, scopes,
      std::move(callback));
}

void DeviceIdentityProvider::InvalidateAccessToken(
    const OAuth2TokenService::ScopeSet& scopes,
    const std::string& access_token) {
  token_service_->InvalidateAccessToken(GetActiveAccountId(), scopes,
                                        access_token);
}

void DeviceIdentityProvider::OnRefreshTokenAvailable(
    const std::string& account_id) {
  ProcessRefreshTokenUpdateForAccount(account_id);
}

void DeviceIdentityProvider::OnRefreshTokenRevoked(
    const std::string& account_id) {
  ProcessRefreshTokenRemovalForAccount(account_id);
}

}  // namespace chromeos
