// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/auth_sync_observer.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/gaia_auth_util.h"

class Profile;

namespace browser_sync {
class ProfileSyncService;
}  // namespace browser_sync

namespace chromeos {

AuthSyncObserver::AuthSyncObserver(Profile* profile)
    : profile_(profile) {
}

AuthSyncObserver::~AuthSyncObserver() {
}

void AuthSyncObserver::StartObserving() {
  browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (sync_service)
    sync_service->AddObserver(this);
}

void AuthSyncObserver::Shutdown() {
  browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (sync_service)
    sync_service->RemoveObserver(this);
}

void AuthSyncObserver::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(user_manager::UserManager::Get()->IsLoggedInAsUserWithGaiaAccount() ||
         user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser());
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);
  GoogleServiceAuthError::State state = sync->GetAuthError().state();
  if (state != GoogleServiceAuthError::NONE &&
      state != GoogleServiceAuthError::CONNECTION_FAILED &&
      state != GoogleServiceAuthError::SERVICE_UNAVAILABLE &&
      state != GoogleServiceAuthError::REQUEST_CANCELED) {
    // Invalidate OAuth2 refresh token to force Gaia sign-in flow. This is
    // needed because sign-out/sign-in solution is suggested to the user.
    // TODO(nkostylev): Remove after crosbug.com/25978 is implemented.
    LOG(WARNING) << "Invalidate OAuth token because of a sync error: "
                 << sync->GetAuthError().ToString();
    const AccountId& account_id = user->GetAccountId();
    DCHECK(account_id.is_valid());
    // TODO(nkostyelv): Change observer after active user has changed.
    user_manager::User::OAuthTokenStatus old_status =
        user->oauth_token_status();
    user_manager::UserManager::Get()->SaveUserOAuthStatus(
        account_id, user_manager::User::OAUTH2_TOKEN_STATUS_INVALID);
    RecordReauthReason(account_id, ReauthReason::SYNC_FAILED);
    if (user->GetType() == user_manager::USER_TYPE_SUPERVISED &&
        old_status != user_manager::User::OAUTH2_TOKEN_STATUS_INVALID) {
       // Attempt to restore token from file.
      ChromeUserManager::Get()
          ->GetSupervisedUserManager()
          ->LoadSupervisedUserToken(
              profile_,
              base::Bind(&AuthSyncObserver::OnSupervisedTokenLoaded,
                         base::Unretained(this)));
      base::RecordAction(
          base::UserMetricsAction("ManagedUsers_Chromeos_Sync_Invalidated"));
    }
  } else if (state == GoogleServiceAuthError::NONE) {
    if (user->GetType() == user_manager::USER_TYPE_SUPERVISED &&
        user->oauth_token_status() ==
            user_manager::User::OAUTH2_TOKEN_STATUS_INVALID) {
      LOG(ERROR) <<
          "Got an incorrectly invalidated token case, restoring token status.";
      user_manager::UserManager::Get()->SaveUserOAuthStatus(
          user->GetAccountId(), user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
      base::RecordAction(
          base::UserMetricsAction("ManagedUsers_Chromeos_Sync_Recovered"));
    }
  }
}

void AuthSyncObserver::OnSupervisedTokenLoaded(const std::string& token) {
  ChromeUserManager::Get()->GetSupervisedUserManager()->ConfigureSyncWithToken(
      profile_, token);
}

}  // namespace chromeos
