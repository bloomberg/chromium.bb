// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/profile_oauth2_token_service.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/device_id_helper.h"
#include "components/signin/core/browser/signin_pref_names.h"

ProfileOAuth2TokenService::ProfileOAuth2TokenService(
    PrefService* user_prefs,
    std::unique_ptr<OAuth2TokenServiceDelegate> delegate)
    : OAuth2TokenService(std::move(delegate)),
      user_prefs_(user_prefs),
      all_credentials_loaded_(false),
      diagnostics_client_(nullptr) {
  DCHECK(user_prefs_);
  AddObserver(this);
}

ProfileOAuth2TokenService::~ProfileOAuth2TokenService() {
  RemoveObserver(this);
}

// static
void ProfileOAuth2TokenService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if defined(OS_IOS)
  registry->RegisterBooleanPref(prefs::kTokenServiceExcludeAllSecondaryAccounts,
                                false);
  registry->RegisterListPref(prefs::kTokenServiceExcludedSecondaryAccounts);
#endif
  registry->RegisterStringPref(prefs::kGoogleServicesSigninScopedDeviceId,
                               std::string());
}

void ProfileOAuth2TokenService::Shutdown() {
  CancelAllRequests();
  GetDelegate()->Shutdown();
}

void ProfileOAuth2TokenService::LoadCredentials(
    const std::string& primary_account_id) {
  GetDelegate()->LoadCredentials(primary_account_id);
}

bool ProfileOAuth2TokenService::AreAllCredentialsLoaded() {
  return all_credentials_loaded_;
}

void ProfileOAuth2TokenService::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  GetDelegate()->UpdateCredentials(account_id, refresh_token);
}

void ProfileOAuth2TokenService::RevokeCredentials(
    const std::string& account_id) {
  GetDelegate()->RevokeCredentials(account_id);
}

const net::BackoffEntry* ProfileOAuth2TokenService::GetDelegateBackoffEntry() {
  return GetDelegate()->BackoffEntry();
}

void ProfileOAuth2TokenService::OnRefreshTokenAvailable(
    const std::string& account_id) {
  // Check if the newly-updated token is valid (invalid tokens are inserted when
  // the user signs out on the web with DICE enabled).
  bool is_valid = true;
  GoogleServiceAuthError token_error = GetAuthError(account_id);
  if (token_error == GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                         GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                             CREDENTIALS_REJECTED_BY_CLIENT)) {
    is_valid = false;
  }

  // NOTE: The code executed in the rest of this method does not affect the
  // state of the accounts in this object, so it doesn't matter whether the
  // callout to |diagnostics_client_| is made before or after. If that fact ever
  // changes, it will be necessary to reason about what the ordering should be.
  if (diagnostics_client_) {
    diagnostics_client_->WillFireOnRefreshTokenAvailable(account_id, is_valid);
  }

  CancelRequestsForAccount(account_id);
  ClearCacheForAccount(account_id);
}

void ProfileOAuth2TokenService::OnRefreshTokenRevoked(
    const std::string& account_id) {
  // If this was the last token, recreate the device ID.
  RecreateDeviceIdIfNeeded();

  // NOTE: The code executed in the rest of this method does not affect the
  // state of the accounts in this object, so it doesn't matter whether the
  // callout to |diagnostics_client_| is made before or after. If that fact ever
  // changes, it will be necessary to reason about what the ordering should be.
  if (diagnostics_client_) {
    diagnostics_client_->WillFireOnRefreshTokenRevoked(account_id);
  }

  CancelRequestsForAccount(account_id);
  ClearCacheForAccount(account_id);
}

void ProfileOAuth2TokenService::OnRefreshTokensLoaded() {
  // Ensure the device ID is not empty, and recreate it if all tokens were
  // cleared during the loading process.
  RecreateDeviceIdIfNeeded();

  all_credentials_loaded_ = true;
}

void ProfileOAuth2TokenService::RecreateDeviceIdIfNeeded() {
// On ChromeOS the device ID is not managed by the token service.
#if !defined(OS_CHROMEOS)
  // Re-create a new device ID if needed.
  switch (GetDelegate()->GetLoadCredentialsState()) {
    case OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_UNKNOWN:
    case OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_NOT_STARTED:
    case OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_IN_PROGRESS:
      // TODO(droger): Add a DCHECK here, because this would mean that the token
      // service is being used before tokens are loaded. This currently would
      // fire in tests though.
      return;
    case OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_FINISHED_WITH_DB_ERRORS:
    case OAuth2TokenServiceDelegate::
        LOAD_CREDENTIALS_FINISHED_WITH_DECRYPT_ERRORS:
      // Do not recreate a new device ID if Chrome fails to decrypt tokens as it
      // may successfully load them on the next restart.
      return;
    case OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS:
    case OAuth2TokenServiceDelegate::
        LOAD_CREDENTIALS_FINISHED_WITH_NO_TOKEN_FOR_PRIMARY_ACCOUNT:
    case OAuth2TokenServiceDelegate::
        LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS:
      // this is the only case when we recreate the device ID.
      if (GetAccounts().empty())
        signin::RecreateSigninScopedDeviceId(user_prefs_);
      return;
  }

  NOTREACHED();
#endif
}
