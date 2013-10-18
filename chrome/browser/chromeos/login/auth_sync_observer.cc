// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth_sync_observer.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/chromeos/login/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "google_apis/gaia/gaia_auth_util.h"

class Profile;
class ProfileSyncService;

namespace chromeos {

AuthSyncObserver::AuthSyncObserver(Profile* profile)
    : profile_(profile) {
}

AuthSyncObserver::~AuthSyncObserver() {
}

void AuthSyncObserver::StartObserving() {
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (sync_service)
    sync_service->AddObserver(this);
}

void AuthSyncObserver::Shutdown() {
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (sync_service)
    sync_service->RemoveObserver(this);
}

void AuthSyncObserver::OnStateChanged() {
  DCHECK(UserManager::Get()->IsLoggedInAsRegularUser() ||
         UserManager::Get()->IsLoggedInAsLocallyManagedUser());
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  GoogleServiceAuthError::State state =
      sync_service->GetAuthError().state();
  if (state != GoogleServiceAuthError::NONE &&
      state != GoogleServiceAuthError::CONNECTION_FAILED &&
      state != GoogleServiceAuthError::SERVICE_UNAVAILABLE &&
      state != GoogleServiceAuthError::REQUEST_CANCELED) {
    // Invalidate OAuth2 refresh token to force Gaia sign-in flow. This is
    // needed because sign-out/sign-in solution is suggested to the user.
    // TODO(nkostylev): Remove after crosbug.com/25978 is implemented.
    LOG(WARNING) << "Invalidate OAuth token because of a sync error: "
                 << sync_service->GetAuthError().ToString();
    std::string email = profile_->GetProfileName();
    if (email.empty() && UserManager::Get()->IsLoggedInAsLocallyManagedUser()) {
      std::string sync_id =
          profile_->GetPrefs()->GetString(prefs::kManagedUserId);
      const User* user =
          UserManager::Get()->GetSupervisedUserManager()->FindBySyncId(sync_id);
      if (user)
        email = user->email();
    }
    DCHECK(!email.empty());
    // TODO(nkostyelv): Change observer after active user has changed.
    UserManager::Get()->SaveUserOAuthStatus(
        gaia::CanonicalizeEmail(email),
        User::OAUTH2_TOKEN_STATUS_INVALID);
  }
}

}  // namespace chromeos
