// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_tracker.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_constants.h"

static const char* kSignedInServices[] = {
  GaiaConstants::kSyncService,
  GaiaConstants::kGaiaOAuth2LoginRefreshToken
};
static const int kNumSignedInServices =
    arraysize(kSignedInServices);

// Helper to check if the given token service is relevant for sync.
SigninTracker::SigninTracker(Profile* profile, Observer* observer)
    : state_(WAITING_FOR_GAIA_VALIDATION),
      profile_(profile),
      observer_(observer),
      credentials_valid_(false) {
  Initialize();
}

SigninTracker::SigninTracker(Profile* profile,
                             Observer* observer,
                             LoginState state)
    : state_(state),
      profile_(profile),
      observer_(observer),
      credentials_valid_(false) {
  Initialize();
}

SigninTracker::~SigninTracker() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (service)
    service->RemoveObserver(this);
}

void SigninTracker::Initialize() {
  DCHECK(observer_);
  // Register for notifications from the SigninManager.
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
                 content::Source<Profile>(profile_));
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 content::Source<TokenService>(token_service));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
                 content::Source<TokenService>(token_service));

  // Also listen for notifications from the various signed in services (only
  // sync for now).
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (service)
    service->AddObserver(this);

  if (state_ == SERVICES_INITIALIZING)
    HandleServiceStateChange();
}

void SigninTracker::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  // We should not get more than one of these notifications.
  switch (type) {
    case chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL:
      DCHECK_EQ(state_, WAITING_FOR_GAIA_VALIDATION);
      state_ = SERVICES_INITIALIZING;
      observer_->GaiaCredentialsValid();
      // If our services are already signed in, see if it's possible to
      // transition to the SIGNIN_COMPLETE state.
      if (AreServicesSignedIn(profile_))
        HandleServiceStateChange();
      break;
    case chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED: {
      DCHECK_EQ(state_, WAITING_FOR_GAIA_VALIDATION);
      const GoogleServiceAuthError& error =
          *(content::Details<const GoogleServiceAuthError>(details).ptr());
      observer_->SigninFailed(error);
      break;
    }
    case chrome::NOTIFICATION_TOKEN_AVAILABLE:
      // A new token is available - check to see if we're all signed in now.
      HandleServiceStateChange();
      break;
    case chrome::NOTIFICATION_TOKEN_REQUEST_FAILED:
      if (state_ == SERVICES_INITIALIZING) {
        const TokenService::TokenRequestFailedDetails& token_details =
            *(content::Details<const TokenService::TokenRequestFailedDetails>(
                details).ptr());
        for (int i = 0; i < kNumSignedInServices; ++i) {
          if (token_details.service() == kSignedInServices[i]) {
            // We got an error loading one of our tokens, so notify our
            // observer.
            state_ = WAITING_FOR_GAIA_VALIDATION;
            observer_->SigninFailed(token_details.error());
          }
        }
      }
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

  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  state_ = GetSigninState(profile_, &error);
  switch (state_) {
    case WAITING_FOR_GAIA_VALIDATION:
      observer_->SigninFailed(error);
      break;
    case SIGNIN_COMPLETE:
      observer_->SigninSuccess();
      break;
    default:
      // State has not changed, nothing to do.
      DCHECK_EQ(state_, SERVICES_INITIALIZING);
      break;
  }
}

// static
bool SigninTracker::AreServiceTokensLoaded(Profile* profile) {
  // See if we have all of the tokens required.
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile);
  for (int i = 0; i < kNumSignedInServices; ++i) {
    if (!token_service->HasTokenForService(kSignedInServices[i])) {
      // Don't have a token for one of our signed-in services.
      return false;
    }
  }
  return true;
}

// static
bool SigninTracker::AreServicesSignedIn(Profile* profile) {
  if (!AreServiceTokensLoaded(profile))
    return false;
  // Don't care about the sync state if sync is disabled by policy.
  if (!profile->IsSyncAccessible())
    return true;
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  return (service->IsSyncEnabledAndLoggedIn() &&
          service->IsSyncTokenAvailable() &&
          service->GetAuthError().state() == GoogleServiceAuthError::NONE &&
          !service->HasUnrecoverableError());
}

// static
SigninTracker::LoginState SigninTracker::GetSigninState(
    Profile* profile,
    GoogleServiceAuthError* error) {
  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile);
  if (signin->GetAuthenticatedUsername().empty()) {
    // User is signed out, trigger a signin failure.
    if (error)
      *error = GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
    return WAITING_FOR_GAIA_VALIDATION;
  }

  // Wait until all of our services are logged in. For now this just means sync.
  // Long term, we should separate out service auth failures from the signin
  // process, but for the current UI flow we'll validate service signin status
  // also.
  ProfileSyncService* service = profile->IsSyncAccessible() ?
      ProfileSyncServiceFactory::GetForProfile(profile) : NULL;
  if (service && service->waiting_for_auth()) {
    // Still waiting for an auth token to come in so stay in the INITIALIZING
    // state (we do this to avoid triggering an early signin error in the case
    // where there's a previous auth error in the sync service that hasn't
    // been cleared yet).
    return SERVICES_INITIALIZING;
  }

  // If we haven't loaded all our service tokens yet, just exit (we'll be called
  // again when another token is loaded, or will transition to SigninFailed if
  // the loading fails).
  if (!AreServiceTokensLoaded(profile))
    return SERVICES_INITIALIZING;
  if (!AreServicesSignedIn(profile)) {
    if (error)
      *error = signin->signin_global_error()->GetLastAuthError();
    return WAITING_FOR_GAIA_VALIDATION;
  }

  if (!service || service->sync_initialized())
    return SIGNIN_COMPLETE;

  return SERVICES_INITIALIZING;
}
