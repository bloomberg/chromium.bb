// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_tracker.h"

#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

SigninTracker::SigninTracker(Profile* profile, Observer* observer)
    : state_(WAITING_FOR_GAIA_VALIDATION),
      profile_(profile),
      observer_(observer),
      credentials_valid_(false) {
  DCHECK(observer_);
  // Register for notifications from the SigninManager.
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
                 content::Source<Profile>(profile_));

  // Also listen for notifications from the various signed in services (only
  // sync for now).
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  service->AddObserver(this);
}

SigninTracker::~SigninTracker() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  service->RemoveObserver(this);
}

void SigninTracker::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  // We should not get more than one of these notifications.
  DCHECK_EQ(state_, WAITING_FOR_GAIA_VALIDATION);
  switch (type) {
    case chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL:
      state_ = SERVICES_INITIALIZING;
      observer_->GaiaCredentialsValid();
      // If our services are already signed in, see if it's possible to
      // transition to the SIGNIN_COMPLETE state.
      if (AreServicesSignedIn(profile_))
        HandleServiceStateChange();
      break;
    case chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED:
      state_ = WAITING_FOR_GAIA_VALIDATION;
      observer_->SigninFailed();
      break;
    default:
      NOTREACHED();
  }
}

// Called when the ProfileSyncService state changes.
void SigninTracker::OnStateChanged() {
  HandleServiceStateChange();
}

void SigninTracker::HandleServiceStateChange() {
  if (state_ != SERVICES_INITIALIZING) {
    // Ignore service updates until after our GAIA credentials are validated.
    return;
  }
  // Wait until all of our services are logged in. For now this just means sync.
  // Long term, we should separate out service auth failures from the signin
  // process, but for the current UI flow we'll validate service signin status
  // also.
  // TODO(atwilson): Move the code to wait for app notification oauth tokens out
  // of ProfileSyncService and over to here (http://crbug.com/114209).
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (service->waiting_for_auth()) {
    // Still waiting for an auth token to come in so stay in the INITIALIZING
    // state (we do this to avoid triggering an early signin error in the case
    // where there's a previous auth error in the sync service that hasn't
    // been cleared yet).
    return;
  }
  if (!AreServicesSignedIn(profile_)) {
    state_ = WAITING_FOR_GAIA_VALIDATION;
    observer_->SigninFailed();
  } else if (service->sync_initialized()) {
    state_ = SIGNIN_COMPLETE;
    observer_->SigninSuccess();
  }
}

// static
bool SigninTracker::AreServicesSignedIn(Profile* profile) {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  return (service->AreCredentialsAvailable() &&
          service->GetAuthError().state() == GoogleServiceAuthError::NONE &&
          !service->unrecoverable_error_detected());
}

