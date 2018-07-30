// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_invalidation_service.h"

#include "components/invalidation/impl/fcm_invalidator.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/invalidation/impl/invalidation_service_util.h"
#include "components/invalidation/impl/invalidator.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/object_id_invalidation_map.h"

const char kFCMOAuthScope[] =
    "https://www.googleapis.com/auth/firebase.messaging";

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
    0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    1000 * 3600 * 4,  // 4 hours.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

namespace invalidation {

FCMInvalidationService::FCMInvalidationService(
    std::unique_ptr<IdentityProvider> identity_provider,
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    PrefService* pref_service,
    const syncer::ParseJSONCallback& parse_json,
    network::mojom::URLLoaderFactory* loader_factory)
    : identity_provider_(std::move(identity_provider)),
      request_access_token_backoff_(&kRequestAccessTokenBackoffPolicy),
      gcm_driver_(gcm_driver),
      instance_id_driver_(instance_id_driver),
      pref_service_(pref_service),
      parse_json_(parse_json),
      loader_factory_(loader_factory) {}

FCMInvalidationService::~FCMInvalidationService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  invalidator_registrar_.UpdateInvalidatorState(
      syncer::INVALIDATOR_SHUTTING_DOWN);
  identity_provider_->RemoveObserver(this);
  if (IsStarted())
    StopInvalidator();
}

void FCMInvalidationService::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsReadyToStart())
    StartInvalidator();

  identity_provider_->AddObserver(this);
}

void FCMInvalidationService::InitForTest(syncer::Invalidator* invalidator) {
  // Here we perform the equivalent of Init() and StartInvalidator(), but with
  // some minor changes to account for the fact that we're injecting the
  // invalidator.
  invalidator_.reset(invalidator);

  invalidator_->RegisterHandler(this);
  CHECK(invalidator_->UpdateRegisteredIds(
      this, invalidator_registrar_.GetAllRegisteredIds()));
}

void FCMInvalidationService::RegisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Registering an invalidation handler";
  invalidator_registrar_.RegisterHandler(handler);
  logger_.OnRegistration(handler->GetOwnerName());
}

bool FCMInvalidationService::UpdateRegisteredInvalidationIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Registering ids: " << ids.size();
  if (!invalidator_registrar_.UpdateRegisteredIds(handler, ids))
    return false;
  if (invalidator_) {
    CHECK(invalidator_->UpdateRegisteredIds(
        this, invalidator_registrar_.GetAllRegisteredIds()));
  }
  logger_.OnUpdateIds(invalidator_registrar_.GetSanitizedHandlersIdsMap());
  return true;
}

void FCMInvalidationService::UnregisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Unregistering";
  invalidator_registrar_.UnregisterHandler(handler);
  if (invalidator_) {
    CHECK(invalidator_->UpdateRegisteredIds(
        this, invalidator_registrar_.GetAllRegisteredIds()));
  }
  logger_.OnUnregistration(handler->GetOwnerName());
}

syncer::InvalidatorState FCMInvalidationService::GetInvalidatorState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (invalidator_) {
    DVLOG(2) << "GetInvalidatorState returning "
             << invalidator_->GetInvalidatorState();
    return invalidator_->GetInvalidatorState();
  }
  DVLOG(2) << "Invalidator currently stopped";
  return syncer::TRANSIENT_INVALIDATION_ERROR;
}

std::string FCMInvalidationService::GetInvalidatorClientId() const {
  NOTREACHED();
  return std::string();
}

InvalidationLogger* FCMInvalidationService::GetInvalidationLogger() {
  return &logger_;
}

void FCMInvalidationService::RequestDetailedStatus(
    base::RepeatingCallback<void(const base::DictionaryValue&)> return_callback)
    const {
  if (IsStarted()) {
    invalidator_->RequestDetailedStatus(return_callback);
  }
}

void FCMInvalidationService::RequestAccessToken() {
  // Only one active request at a time.
  if (access_token_fetcher_ != nullptr)
    return;
  request_access_token_retry_timer_.Stop();
  OAuth2TokenService::ScopeSet oauth2_scopes = {kFCMOAuthScope};
  // Invalidate previous token, otherwise the identity provider will return the
  // same token again.
  identity_provider_->InvalidateAccessToken(oauth2_scopes, access_token_);
  access_token_.clear();
  access_token_fetcher_ = identity_provider_->FetchAccessToken(
      "fcm_invalidation", oauth2_scopes,
      base::BindOnce(&FCMInvalidationService::OnAccessTokenRequestCompleted,
                     base::Unretained(this)));
}

void FCMInvalidationService::OnAccessTokenRequestCompleted(
    GoogleServiceAuthError error,
    std::string access_token) {
  access_token_fetcher_.reset();
  if (error.state() == GoogleServiceAuthError::NONE)
    OnAccessTokenRequestSucceeded(access_token);
  else
    OnAccessTokenRequestFailed(error);
}

void FCMInvalidationService::OnAccessTokenRequestSucceeded(
    std::string access_token) {
  // Reset backoff time after successful response.
  request_access_token_backoff_.Reset();
  access_token_ = access_token;
  if (!IsStarted() && IsReadyToStart()) {
    StartInvalidator();
  } else {
    UpdateInvalidatorCredentials();
  }
}

void FCMInvalidationService::OnAccessTokenRequestFailed(
    GoogleServiceAuthError error) {
  DCHECK_NE(error.state(), GoogleServiceAuthError::NONE);
  switch (error.state()) {
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS: {
      invalidator_registrar_.UpdateInvalidatorState(
          syncer::INVALIDATION_CREDENTIALS_REJECTED);
      break;
    }
    default: {
      request_access_token_backoff_.InformOfRequest(false);
      request_access_token_retry_timer_.Start(
          FROM_HERE, request_access_token_backoff_.GetTimeUntilRelease(),
          base::BindRepeating(&FCMInvalidationService::RequestAccessToken,
                              base::Unretained(this)));
      break;
    }
  }
}

void FCMInvalidationService::OnActiveAccountLogin() {
  if (!IsStarted() && IsReadyToStart())
    StartInvalidator();
}

void FCMInvalidationService::OnActiveAccountRefreshTokenUpdated() {
  if (!IsStarted() && IsReadyToStart())
    StartInvalidator();
  else
    UpdateInvalidatorCredentials();
}

void FCMInvalidationService::OnActiveAccountRefreshTokenRemoved() {
  access_token_.clear();
  if (IsStarted())
    UpdateInvalidatorCredentials();
}

void FCMInvalidationService::OnActiveAccountLogout() {
  access_token_fetcher_.reset();
  request_access_token_retry_timer_.Stop();

  if (IsStarted()) {
    StopInvalidator();
  }
}

void FCMInvalidationService::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  invalidator_registrar_.UpdateInvalidatorState(state);
  logger_.OnStateChange(state);
}

void FCMInvalidationService::OnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  invalidator_registrar_.DispatchInvalidationsToHandlers(invalidation_map);

  logger_.OnInvalidation(invalidation_map);
}

std::string FCMInvalidationService::GetOwnerName() const {
  return "FCM";
}

bool FCMInvalidationService::IsReadyToStart() {
  if (!identity_provider_->IsActiveAccountAvailable()) {
    DVLOG(2) << "Not starting FCMInvalidationService: "
             << "active account is not available";
    return false;
  }

  return true;
}

bool FCMInvalidationService::IsStarted() const {
  return invalidator_ != nullptr;
}

void FCMInvalidationService::StartInvalidator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!invalidator_);
  DCHECK(IsReadyToStart());

  // access token before sending message to server.
  if (access_token_.empty()) {
    // TODO(melandory): move logic for recieving the token directly to
    // the PerUserTopicRegistrationmanager.
    DVLOG(1) << "FCMInvalidationService: "
             << "Deferring start until we have an access token.";
    RequestAccessToken();
    return;
  }

  auto network = std::make_unique<syncer::FCMNetworkHandler>(
      gcm_driver_, instance_id_driver_);
  network->StartListening();
  invalidator_ = std::make_unique<syncer::FCMInvalidator>(
      std::move(network), pref_service_, loader_factory_, parse_json_);

  invalidator_->RegisterHandler(this);
  UpdateInvalidatorCredentials();
  CHECK(invalidator_->UpdateRegisteredIds(
      this, invalidator_registrar_.GetAllRegisteredIds()));
}

void FCMInvalidationService::UpdateInvalidatorCredentials() {
  std::string email = identity_provider_->GetActiveAccountId();

  DCHECK(!email.empty()) << "Expected user to be signed in.";

  invalidator_->UpdateCredentials(email, access_token_);
}

void FCMInvalidationService::StopInvalidator() {
  DCHECK(invalidator_);
  // TODO(melandory): reset the network.
  invalidator_->UnregisterHandler(this);
  invalidator_.reset();
}

}  // namespace invalidation
