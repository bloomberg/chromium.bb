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
#include "google_apis/gaia/gaia_constants.h"

namespace invalidation {

FCMInvalidationService::FCMInvalidationService(
    std::unique_ptr<IdentityProvider> identity_provider,
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    PrefService* pref_service,
    const syncer::ParseJSONCallback& parse_json,
    network::mojom::URLLoaderFactory* loader_factory)
    : gcm_driver_(gcm_driver),
      instance_id_driver_(instance_id_driver),
      identity_provider_(std::move(identity_provider)),
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

void FCMInvalidationService::OnActiveAccountLogin() {
  if (!IsStarted() && IsReadyToStart())
    StartInvalidator();
}

void FCMInvalidationService::OnActiveAccountRefreshTokenUpdated() {
  if (!IsStarted() && IsReadyToStart())
    StartInvalidator();
}

void FCMInvalidationService::OnActiveAccountLogout() {
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

  auto network = std::make_unique<syncer::FCMNetworkHandler>(
      gcm_driver_, instance_id_driver_);
  network->StartListening();
  invalidator_ = std::make_unique<syncer::FCMInvalidator>(
      std::move(network), identity_provider_.get(), pref_service_,
      loader_factory_, parse_json_);

  invalidator_->RegisterHandler(this);
  CHECK(invalidator_->UpdateRegisteredIds(
      this, invalidator_registrar_.GetAllRegisteredIds()));
}

void FCMInvalidationService::StopInvalidator() {
  DCHECK(invalidator_);
  // TODO(melandory): reset the network.
  invalidator_->UnregisterHandler(this);
  invalidator_.reset();
}

}  // namespace invalidation
