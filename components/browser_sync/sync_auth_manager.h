// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_SYNC_AUTH_MANAGER_H_
#define COMPONENTS_BROWSER_SYNC_SYNC_AUTH_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "components/signin/core/browser/account_info.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace browser_sync {

class ProfileSyncService;

// SyncAuthManager tracks the primary (i.e. blessed-for-sync) account and its
// authentication state.
class SyncAuthManager : public identity::IdentityManager::Observer,
                        public OAuth2TokenService::Observer {
 public:
  // |sync_service| must not be null and must outlive this object.
  // |identity_manager| and |token_service| may be null (this is the case if
  // local Sync is enabled), but if non-null, must outlive this object.
  // TODO(crbug.com/842697): Don't pass the ProfileSyncService in here. Instead,
  // pass a callback ("AccountStateChanged(new_state)").
  SyncAuthManager(ProfileSyncService* sync_service,
                  identity::IdentityManager* identity_manager,
                  OAuth2TokenService* token_service);
  ~SyncAuthManager() override;

  // Tells the tracker to start listening for changes to the account/sign-in
  // status. This gets called during SyncService initialization, except in the
  // case of local Sync.
  void RegisterForAuthNotifications();

  // Returns the AccountInfo for the primary (i.e. blessed-for-sync) account, or
  // an empty AccountInfo if there isn't one.
  AccountInfo GetAuthenticatedAccountInfo() const;

  // Returns whether a refresh token is available for the primary account.
  bool RefreshTokenIsAvailable() const;

  const GoogleServiceAuthError& GetLastAuthError() const {
    return last_auth_error_;
  }
  bool IsAuthInProgress() const { return is_auth_in_progress_; }

  // TODO(crbug.com/842697): These shouldn't be necessary anymore once all
  // auth-related things have been moved here from ProfileSyncService.
  void UpdateAuthErrorState(const GoogleServiceAuthError& error);
  void ClearAuthError();

  // identity::IdentityManager::Observer implementation.
  void OnPrimaryAccountSet(const AccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const AccountInfo& previous_primary_account_info) override;

  // OAuth2TokenService::Observer implementation.
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokenRevoked(const std::string& account_id) override;
  void OnRefreshTokensLoaded() override;

 private:
  ProfileSyncService* const sync_service_;
  identity::IdentityManager* const identity_manager_;
  OAuth2TokenService* const token_service_;

  bool registered_for_auth_notifications_;

  // This is a cache of the last authentication response we received either
  // from the sync server or from Chrome's identity/token management system.
  GoogleServiceAuthError last_auth_error_;

  // Set to true if a signin has completed but we're still waiting for the
  // engine to refresh its credentials.
  bool is_auth_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(SyncAuthManager);
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_SYNC_AUTH_MANAGER_H_
