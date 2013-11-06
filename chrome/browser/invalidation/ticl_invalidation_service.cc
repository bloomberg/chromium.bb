// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/ticl_invalidation_service.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/invalidation/invalidation_service_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/about_signin_internals.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "sync/notifier/invalidator.h"
#include "sync/notifier/invalidator_state.h"
#include "sync/notifier/non_blocking_invalidator.h"

static const char* kOAuth2Scopes[] = {
  GaiaConstants::kGoogleTalkOAuth2Scope
};

static const net::BackoffEntry::Policy kRequestAccessTokenBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  2000,

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.2, // 20%

  // Maximum amount of time we are willing to delay our request in ms.
  // TODO(pavely): crbug.com/246686 ProfileSyncService should retry
  // RequestAccessToken on connection state change after backoff
  1000 * 3600 * 4, // 4 hours.

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

namespace invalidation {

TiclInvalidationService::TiclInvalidationService(
    SigninManagerBase* signin,
    ProfileOAuth2TokenService* oauth2_token_service,
    Profile* profile)
    : profile_(profile),
      signin_manager_(signin),
      oauth2_token_service_(oauth2_token_service),
      invalidator_registrar_(new syncer::InvalidatorRegistrar()),
      request_access_token_backoff_(&kRequestAccessTokenBackoffPolicy) {
}

TiclInvalidationService::~TiclInvalidationService() {
  DCHECK(CalledOnValidThread());
}

void TiclInvalidationService::Init() {
  DCHECK(CalledOnValidThread());

  invalidator_storage_.reset(new InvalidatorStorage(profile_->GetPrefs()));
  if (invalidator_storage_->GetInvalidatorClientId().empty()) {
    // This also clears any existing state.  We can't reuse old invalidator
    // state with the new ID anyway.
    invalidator_storage_->SetInvalidatorClientId(GenerateInvalidatorClientId());
  }

  if (IsReadyToStart()) {
    StartInvalidator();
  }

  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                              content::Source<Profile>(profile_));
  oauth2_token_service_->AddObserver(this);
}

void TiclInvalidationService::InitForTest(syncer::Invalidator* invalidator) {
  // Here we perform the equivalent of Init() and StartInvalidator(), but with
  // some minor changes to account for the fact that we're injecting the
  // invalidator.
  invalidator_.reset(invalidator);

  invalidator_->RegisterHandler(this);
  invalidator_->UpdateRegisteredIds(
      this,
      invalidator_registrar_->GetAllRegisteredIds());
}

void TiclInvalidationService::RegisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Registering an invalidation handler";
  invalidator_registrar_->RegisterHandler(handler);
}

void TiclInvalidationService::UpdateRegisteredInvalidationIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Registering ids: " << ids.size();
  invalidator_registrar_->UpdateRegisteredIds(handler, ids);
  if (invalidator_) {
    invalidator_->UpdateRegisteredIds(
        this,
        invalidator_registrar_->GetAllRegisteredIds());
  }
}

void TiclInvalidationService::UnregisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Unregistering";
  invalidator_registrar_->UnregisterHandler(handler);
  if (invalidator_) {
    invalidator_->UpdateRegisteredIds(
        this,
        invalidator_registrar_->GetAllRegisteredIds());
  }
}

void TiclInvalidationService::AcknowledgeInvalidation(
    const invalidation::ObjectId& id,
    const syncer::AckHandle& ack_handle) {
  DCHECK(CalledOnValidThread());
  if (invalidator_) {
    invalidator_->Acknowledge(id, ack_handle);
  }
}

syncer::InvalidatorState TiclInvalidationService::GetInvalidatorState() const {
  DCHECK(CalledOnValidThread());
  if (invalidator_) {
    DVLOG(2) << "GetInvalidatorState returning "
        << invalidator_->GetInvalidatorState();
    return invalidator_->GetInvalidatorState();
  } else {
    DVLOG(2) << "Invalidator currently stopped";
    return syncer::TRANSIENT_INVALIDATION_ERROR;
  }
}

std::string TiclInvalidationService::GetInvalidatorClientId() const {
  DCHECK(CalledOnValidThread());
  return invalidator_storage_->GetInvalidatorClientId();
}

void TiclInvalidationService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, chrome::NOTIFICATION_GOOGLE_SIGNED_OUT);
  Logout();
}

void TiclInvalidationService::RequestAccessToken() {
  // Only one active request at a time.
  if (access_token_request_ != NULL)
    return;
  request_access_token_retry_timer_.Stop();
  OAuth2TokenService::ScopeSet oauth2_scopes;
  for (size_t i = 0; i < arraysize(kOAuth2Scopes); i++)
    oauth2_scopes.insert(kOAuth2Scopes[i]);
  // Invalidate previous token, otherwise token service will return the same
  // token again.
  const std::string& account_id = oauth2_token_service_->GetPrimaryAccountId();
  oauth2_token_service_->InvalidateToken(account_id,
                                         oauth2_scopes,
                                         access_token_);
  access_token_.clear();
  access_token_request_ = oauth2_token_service_->StartRequest(account_id,
                                                              oauth2_scopes,
                                                              this);
}

void TiclInvalidationService::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(access_token_request_, request);
  access_token_request_.reset();
  // Reset backoff time after successful response.
  request_access_token_backoff_.Reset();
  access_token_ = access_token;
  if (!IsStarted() && IsReadyToStart()) {
    StartInvalidator();
  } else {
    UpdateInvalidatorCredentials();
  }
}

void TiclInvalidationService::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(access_token_request_, request);
  DCHECK_NE(error.state(), GoogleServiceAuthError::NONE);
  access_token_request_.reset();
  switch (error.state()) {
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE: {
      // Transient error. Retry after some time.
      request_access_token_backoff_.InformOfRequest(false);
      request_access_token_retry_timer_.Start(
            FROM_HERE,
            request_access_token_backoff_.GetTimeUntilRelease(),
            base::Bind(&TiclInvalidationService::RequestAccessToken,
                       base::Unretained(this)));
      break;
    }
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS: {
      // This is a real auth error.
      // Report time since token was issued for invalid credentials error.
      base::Time auth_token_time =
          AboutSigninInternalsFactory::GetForProfile(profile_)->
              GetTokenTime(GaiaConstants::kGaiaOAuth2LoginRefreshToken);
      if (!auth_token_time.is_null()) {
        base::TimeDelta age = base::Time::Now() - auth_token_time;
        if (age < base::TimeDelta::FromHours(1)) {
          UMA_HISTOGRAM_CUSTOM_TIMES(
              "Sync.AuthInvalidationRejectedTokenAgeShort",
              age,
              base::TimeDelta::FromSeconds(1),
              base::TimeDelta::FromHours(1),
              50);
        }
        UMA_HISTOGRAM_COUNTS("Sync.AuthInvalidationRejectedTokenAgeLong",
                             age.InDays());
      }
      invalidator_registrar_->UpdateInvalidatorState(
          syncer::INVALIDATION_CREDENTIALS_REJECTED);
      break;
    }
    default: {
      // We have no way to notify the user of this.  Do nothing.
    }
  }
}

void TiclInvalidationService::OnRefreshTokenAvailable(
    const std::string& account_id) {
  if (oauth2_token_service_->GetPrimaryAccountId() == account_id) {
    if (!IsStarted() && IsReadyToStart()) {
      StartInvalidator();
    }
  }
}

void TiclInvalidationService::OnRefreshTokenRevoked(
    const std::string& account_id) {
  if (oauth2_token_service_->GetPrimaryAccountId() == account_id) {
    access_token_.clear();
    if (IsStarted()) {
      UpdateInvalidatorCredentials();
    }
  }
}

void TiclInvalidationService::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  if (state == syncer::INVALIDATION_CREDENTIALS_REJECTED) {
    // This may be due to normal OAuth access token expiration.  If so, we must
    // fetch a new one using our refresh token.  Resetting the invalidator's
    // access token will not reset the invalidator's exponential backoff, so
    // it's safe to try to update the token every time we receive this signal.
    //
    // We won't be receiving any invalidations while the refresh is in progress,
    // we set our state to TRANSIENT_INVALIDATION_ERROR.  If the credentials
    // really are invalid, the refresh request should fail and
    // OnGetTokenFailure() will put us into a INVALIDATION_CREDENTIALS_REJECTED
    // state.
    invalidator_registrar_->UpdateInvalidatorState(
        syncer::TRANSIENT_INVALIDATION_ERROR);
    RequestAccessToken();
  } else {
    invalidator_registrar_->UpdateInvalidatorState(state);
  }
}

void TiclInvalidationService::OnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  invalidator_registrar_->DispatchInvalidationsToHandlers(invalidation_map);
}

void TiclInvalidationService::Shutdown() {
  DCHECK(CalledOnValidThread());
  oauth2_token_service_->RemoveObserver(this);
  if (IsStarted()) {
    StopInvalidator();
  }
  invalidator_storage_.reset();
  invalidator_registrar_.reset();
}

bool TiclInvalidationService::IsReadyToStart() {
  if (profile_->IsManaged()) {
    DVLOG(2) << "Not starting TiclInvalidationService: User is managed.";
    return false;
  }

  if (signin_manager_->GetAuthenticatedUsername().empty()) {
    DVLOG(2) << "Not starting TiclInvalidationService: User is not signed in.";
    return false;
  }

  if (!oauth2_token_service_) {
    DVLOG(2)
        << "Not starting TiclInvalidationService: TokenService unavailable.";
    return false;
  }

  if (!oauth2_token_service_->RefreshTokenIsAvailable(
          oauth2_token_service_->GetPrimaryAccountId())) {
    DVLOG(2)
        << "Not starting TiclInvalidationServce: Waiting for refresh token.";
    return false;
  }

  return true;
}

bool TiclInvalidationService::IsStarted() {
  return invalidator_.get() != NULL;
}

void TiclInvalidationService::StartInvalidator() {
  DCHECK(CalledOnValidThread());
  DCHECK(!invalidator_);
  DCHECK(invalidator_storage_);
  DCHECK(!invalidator_storage_->GetInvalidatorClientId().empty());

  if (access_token_.empty()) {
    DVLOG(1)
        << "TiclInvalidationService: "
        << "Deferring start until we have an access token.";
    RequestAccessToken();
    return;
  }

  notifier::NotifierOptions options =
      ParseNotifierOptions(*CommandLine::ForCurrentProcess());
  options.request_context_getter = profile_->GetRequestContext();
  options.auth_mechanism = "X-OAUTH2";
  invalidator_.reset(new syncer::NonBlockingInvalidator(
          options,
          invalidator_storage_->GetInvalidatorClientId(),
          invalidator_storage_->GetAllInvalidationStates(),
          invalidator_storage_->GetBootstrapData(),
          syncer::WeakHandle<syncer::InvalidationStateTracker>(
              invalidator_storage_->AsWeakPtr()),
          content::GetUserAgent(GURL())));

  UpdateInvalidatorCredentials();

  invalidator_->RegisterHandler(this);
  invalidator_->UpdateRegisteredIds(
      this,
      invalidator_registrar_->GetAllRegisteredIds());
}

void TiclInvalidationService::UpdateInvalidatorCredentials() {
  std::string email = signin_manager_->GetAuthenticatedUsername();

  DCHECK(!email.empty()) << "Expected user to be signed in.";

  DVLOG(2) << "UpdateCredentials: " << email;
  invalidator_->UpdateCredentials(email, access_token_);
}

void TiclInvalidationService::StopInvalidator() {
  DCHECK(invalidator_);
  invalidator_->UnregisterHandler(this);
  invalidator_.reset();
}

void TiclInvalidationService::Logout() {
  access_token_request_.reset();
  request_access_token_retry_timer_.Stop();

  if (IsStarted()) {
    StopInvalidator();
  }

  // This service always expects to have a valid invalidator storage.
  // So we must not only clear the old one, but also start a new one.
  invalidator_storage_->Clear();
  invalidator_storage_.reset(new InvalidatorStorage(profile_->GetPrefs()));
  invalidator_storage_->SetInvalidatorClientId(GenerateInvalidatorClientId());
}

}  // namespace invalidation
