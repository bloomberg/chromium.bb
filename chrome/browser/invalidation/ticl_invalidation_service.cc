// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/ticl_invalidation_service.h"

#include "base/command_line.h"
#include "chrome/browser/invalidation/invalidation_service_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "sync/notifier/invalidator.h"
#include "sync/notifier/invalidator_state.h"
#include "sync/notifier/non_blocking_invalidator.h"

namespace invalidation {

TiclInvalidationService::TiclInvalidationService(SigninManagerBase* signin,
                                                 TokenService* token_service,
                                                 Profile* profile)
  : profile_(profile),
    signin_manager_(signin),
    token_service_(token_service),
    invalidator_registrar_(new syncer::InvalidatorRegistrar()) { }

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
    Start();
  }

  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_TOKEN_AVAILABLE,
                              content::Source<TokenService>(token_service_));
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                              content::Source<Profile>(profile_));
}

void TiclInvalidationService::InitForTest(syncer::Invalidator* invalidator) {
  // Here we perform the equivalent of Init() and Start(), but with some minor
  // changes to account for the fact that we're injecting the invalidator.
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

  switch (type) {
    case chrome::NOTIFICATION_TOKEN_AVAILABLE: {
      const TokenService::TokenAvailableDetails& token_details =
          *(content::Details<const TokenService::TokenAvailableDetails>(
                  details).ptr());
      if (token_details.service() == GaiaConstants::kSyncService) {
        DCHECK(IsReadyToStart());
        if (!IsStarted()) {
          Start();
        } else {
          UpdateToken();
        }
      }
      break;
    }
    case chrome::NOTIFICATION_GOOGLE_SIGNED_OUT: {
      Logout();
      break;
    }
    default: {
      NOTREACHED();
    }
  }
}

void TiclInvalidationService::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  invalidator_registrar_->UpdateInvalidatorState(state);
}

void TiclInvalidationService::OnIncomingInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {
  invalidator_registrar_->DispatchInvalidationsToHandlers(invalidation_map);
}

void TiclInvalidationService::Shutdown() {
  DCHECK(CalledOnValidThread());
  if (IsStarted()) {
    StopInvalidator();
  }
  invalidator_storage_.reset();
  invalidator_registrar_.reset();
}

bool TiclInvalidationService::IsReadyToStart() {
  if (signin_manager_->GetAuthenticatedUsername().empty()) {
    DVLOG(2) << "Not starting TiclInvalidationService: user is not signed in.";
    return false;
  }

  if (!token_service_) {
    DVLOG(2)
        << "Not starting TiclInvalidationService: TokenService unavailable.";
    return false;
  }

  if (!token_service_->HasTokenForService(GaiaConstants::kSyncService)) {
    DVLOG(2) << "Not starting TiclInvalidationService: Sync token unavailable.";
    return false;
  }

  return true;
}

bool TiclInvalidationService::IsStarted() {
  return invalidator_.get() != NULL;
}

void TiclInvalidationService::Start() {
  DCHECK(CalledOnValidThread());
  DCHECK(!invalidator_);
  DCHECK(invalidator_storage_);
  DCHECK(!invalidator_storage_->GetInvalidatorClientId().empty());

  notifier::NotifierOptions options =
      ParseNotifierOptions(*CommandLine::ForCurrentProcess());
  options.request_context_getter = profile_->GetRequestContext();
  invalidator_.reset(new syncer::NonBlockingInvalidator(
          options,
          invalidator_storage_->GetInvalidatorClientId(),
          invalidator_storage_->GetAllInvalidationStates(),
          invalidator_storage_->GetBootstrapData(),
          syncer::WeakHandle<syncer::InvalidationStateTracker>(
              invalidator_storage_->AsWeakPtr()),
          content::GetUserAgent(GURL())));

  UpdateToken();

  invalidator_->RegisterHandler(this);
  invalidator_->UpdateRegisteredIds(
      this,
      invalidator_registrar_->GetAllRegisteredIds());
}

void TiclInvalidationService::UpdateToken() {
  std::string email = signin_manager_->GetAuthenticatedUsername();
  DCHECK(!email.empty()) << "Expected user to be signed in.";
  DCHECK(token_service_->HasTokenForService(GaiaConstants::kSyncService));

  std::string sync_token = token_service_->GetTokenForService(
      GaiaConstants::kSyncService);

  DVLOG(2) << "UpdateCredentials: " << email;
  invalidator_->UpdateCredentials(email, sync_token);
}

void TiclInvalidationService::StopInvalidator() {
  DCHECK(invalidator_);
  invalidator_->UnregisterHandler(this);
  invalidator_.reset();
}

void TiclInvalidationService::Logout() {
  StopInvalidator();

  // This service always expects to have a valid invalidator storage.
  // So we must not only clear the old one, but also start a new one.
  invalidator_storage_->Clear();
  invalidator_storage_.reset(new InvalidatorStorage(profile_->GetPrefs()));
  invalidator_storage_->SetInvalidatorClientId(GenerateInvalidatorClientId());
}

}  // namespace invalidation
