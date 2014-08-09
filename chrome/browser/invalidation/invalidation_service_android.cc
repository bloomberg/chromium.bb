// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/invalidation_service_android.h"

#include "base/callback.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/invalidation/invalidation_controller_android.h"
#include "components/invalidation/object_id_invalidation_map.h"
#include "content/public/browser/notification_service.h"

namespace invalidation {

InvalidationServiceAndroid::InvalidationServiceAndroid(
    Profile* profile,
    InvalidationControllerAndroid* invalidation_controller)
    : invalidator_state_(syncer::INVALIDATIONS_ENABLED),
      invalidation_controller_(invalidation_controller),
      logger_() {
  DCHECK(CalledOnValidThread());
  DCHECK(invalidation_controller);
  registrar_.Add(this, chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
                 content::Source<Profile>(profile));
}

InvalidationServiceAndroid::~InvalidationServiceAndroid() { }

void InvalidationServiceAndroid::RegisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK(CalledOnValidThread());
  invalidator_registrar_.RegisterHandler(handler);
  logger_.OnRegistration(handler->GetOwnerName());
}

void InvalidationServiceAndroid::UpdateRegisteredInvalidationIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  DCHECK(CalledOnValidThread());
  invalidator_registrar_.UpdateRegisteredIds(handler, ids);
  invalidation_controller_->SetRegisteredObjectIds(
      invalidator_registrar_.GetAllRegisteredIds());
  logger_.OnUpdateIds(invalidator_registrar_.GetSanitizedHandlersIdsMap());
}

void InvalidationServiceAndroid::UnregisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK(CalledOnValidThread());
  invalidator_registrar_.UnregisterHandler(handler);
  logger_.OnUnregistration(handler->GetOwnerName());
}

syncer::InvalidatorState
InvalidationServiceAndroid::GetInvalidatorState() const {
  DCHECK(CalledOnValidThread());
  return invalidator_state_;
}

std::string InvalidationServiceAndroid::GetInvalidatorClientId() const {
  DCHECK(CalledOnValidThread());
  return invalidation_controller_->GetInvalidatorClientId();
}

InvalidationLogger* InvalidationServiceAndroid::GetInvalidationLogger() {
  return &logger_;
}

void InvalidationServiceAndroid::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> return_callback) const {
}

IdentityProvider* InvalidationServiceAndroid::GetIdentityProvider() {
  return NULL;
}

void InvalidationServiceAndroid::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type, chrome::NOTIFICATION_SYNC_REFRESH_REMOTE);

  content::Details<const syncer::ObjectIdInvalidationMap>
      state_details(details);
  const syncer::ObjectIdInvalidationMap object_invalidation_map =
      *(state_details.ptr());

  // An empty map implies that we should invalidate all.
  const syncer::ObjectIdInvalidationMap& effective_invalidation_map =
      object_invalidation_map.Empty() ?
      syncer::ObjectIdInvalidationMap::InvalidateAll(
          invalidator_registrar_.GetAllRegisteredIds()) :
      object_invalidation_map;

  invalidator_registrar_.DispatchInvalidationsToHandlers(
      effective_invalidation_map);
  logger_.OnInvalidation(effective_invalidation_map);
}

void InvalidationServiceAndroid::TriggerStateChangeForTest(
    syncer::InvalidatorState state) {
  DCHECK(CalledOnValidThread());
  invalidator_state_ = state;
  invalidator_registrar_.UpdateInvalidatorState(invalidator_state_);
}

} //  namespace invalidation
