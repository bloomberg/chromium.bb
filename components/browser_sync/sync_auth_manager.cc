// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_auth_manager.h"

#include "base/metrics/histogram_macros.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/base/stop_source.h"

namespace browser_sync {

SyncAuthManager::SyncAuthManager(ProfileSyncService* sync_service,
                                 identity::IdentityManager* identity_manager,
                                 OAuth2TokenService* token_service)
    : sync_service_(sync_service),
      identity_manager_(identity_manager),
      token_service_(token_service),
      registered_for_auth_notifications_(false),
      is_auth_in_progress_(false) {
  DCHECK(sync_service_);
  // |identity_manager_| and |token_service_| can be null if local Sync is
  // enabled.
}

void SyncAuthManager::RegisterForAuthNotifications() {
  DCHECK(!registered_for_auth_notifications_);
  identity_manager_->AddObserver(this);
  token_service_->AddObserver(this);
  registered_for_auth_notifications_ = true;
}

SyncAuthManager::~SyncAuthManager() {
  if (registered_for_auth_notifications_) {
    token_service_->RemoveObserver(this);
    identity_manager_->RemoveObserver(this);
  }
}

AccountInfo SyncAuthManager::GetAuthenticatedAccountInfo() const {
  return identity_manager_ ? identity_manager_->GetPrimaryAccountInfo()
                           : AccountInfo();
}

bool SyncAuthManager::RefreshTokenIsAvailable() const {
  std::string account_id = GetAuthenticatedAccountInfo().account_id;
  return !account_id.empty() &&
         token_service_->RefreshTokenIsAvailable(account_id);
}

void SyncAuthManager::UpdateAuthErrorState(
    const GoogleServiceAuthError& error) {
  is_auth_in_progress_ = false;
  last_auth_error_ = error;
}

void SyncAuthManager::ClearAuthError() {
  UpdateAuthErrorState(GoogleServiceAuthError::AuthErrorNone());
}

void SyncAuthManager::OnPrimaryAccountSet(
    const AccountInfo& primary_account_info) {
  // Track the fact that we're still waiting for auth to complete.
  DCHECK(!is_auth_in_progress_);
  is_auth_in_progress_ = true;

  sync_service_->OnPrimaryAccountSet();

  if (token_service_->RefreshTokenIsAvailable(
          primary_account_info.account_id)) {
    OnRefreshTokenAvailable(primary_account_info.account_id);
  }
}

void SyncAuthManager::OnPrimaryAccountCleared(
    const AccountInfo& previous_primary_account_info) {
  UMA_HISTOGRAM_ENUMERATION("Sync.StopSource", syncer::SIGN_OUT,
                            syncer::STOP_SOURCE_LIMIT);
  is_auth_in_progress_ = false;
  sync_service_->OnPrimaryAccountCleared();
}

void SyncAuthManager::OnRefreshTokenAvailable(const std::string& account_id) {
  if (account_id == GetAuthenticatedAccountInfo().account_id) {
    sync_service_->OnRefreshTokenAvailable();
  }
}

void SyncAuthManager::OnRefreshTokenRevoked(const std::string& account_id) {
  if (account_id == GetAuthenticatedAccountInfo().account_id) {
    UpdateAuthErrorState(
        GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
    sync_service_->OnRefreshTokenRevoked();
  }
}

void SyncAuthManager::OnRefreshTokensLoaded() {
  GoogleServiceAuthError token_error =
      token_service_->GetAuthError(GetAuthenticatedAccountInfo().account_id);
  if (token_error == GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                         GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                             CREDENTIALS_REJECTED_BY_CLIENT)) {
    is_auth_in_progress_ = false;
    // When the refresh token is replaced by a new token with a
    // CREDENTIALS_REJECTED_BY_CLIENT error, Sync must be stopped immediately,
    // even if the current access token is still valid. This happens e.g. when
    // the user signs out of the web with Dice enabled.
    // It is not necessary to do this when the refresh token is
    // CREDENTIALS_REJECTED_BY_SERVER, because in that case the access token
    // will be rejected by the server too.
    // We only do this in OnRefreshTokensLoaded(), as opposed to
    // OAuth2TokenService::Observer::OnAuthErrorChanged(), because
    // CREDENTIALS_REJECTED_BY_CLIENT is only set by the signin component when
    // the refresh token is created.
    sync_service_->OnCredentialsRejectedByClient();
  }

  // This notification gets fired when OAuth2TokenService loads the tokens from
  // storage. Initialize the engine if sync is enabled. If the sync token was
  // not loaded, GetCredentials() will generate invalid credentials to cause the
  // engine to generate an auth error (https://crbug.com/121755).
  // TODO(treib): Is this necessary? Either we actually have a refresh token, in
  // which case this was already called from OnRefreshTokenAvailable above, or
  // there is no refresh token, in which case Sync can't start anyway.
  sync_service_->OnRefreshTokenAvailable();
}

}  // namespace browser_sync
