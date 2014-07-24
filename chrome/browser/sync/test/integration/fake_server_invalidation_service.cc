// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/fake_server_invalidation_service.h"

#include <string>

#include "base/macros.h"
#include "chrome/browser/sync/glue/invalidation_helper.h"
#include "components/invalidation/invalidation.h"
#include "components/invalidation/invalidation_service_util.h"
#include "components/invalidation/object_id_invalidation_map.h"
#include "sync/internal_api/public/base/model_type.h"

namespace fake_server {

FakeServerInvalidationService::FakeServerInvalidationService()
    : client_id_(invalidation::GenerateInvalidatorClientId()),
      self_notify_(true),
      identity_provider_(&token_service_) {
  invalidator_registrar_.UpdateInvalidatorState(syncer::INVALIDATIONS_ENABLED);
}

FakeServerInvalidationService::~FakeServerInvalidationService() {
}

void FakeServerInvalidationService::RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) {
  invalidator_registrar_.RegisterHandler(handler);
}

void FakeServerInvalidationService::UpdateRegisteredInvalidationIds(
      syncer::InvalidationHandler* handler,
      const syncer::ObjectIdSet& ids) {
  invalidator_registrar_.UpdateRegisteredIds(handler, ids);
}

void FakeServerInvalidationService::UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) {
  invalidator_registrar_.UnregisterHandler(handler);
}

syncer::InvalidatorState FakeServerInvalidationService::GetInvalidatorState()
    const {
  return invalidator_registrar_.GetInvalidatorState();
}

std::string FakeServerInvalidationService::GetInvalidatorClientId() const {
  return client_id_;
}

invalidation::InvalidationLogger*
FakeServerInvalidationService::GetInvalidationLogger() {
  return NULL;
}

void FakeServerInvalidationService::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> caller) const {
  base::DictionaryValue value;
  caller.Run(value);
}

IdentityProvider* FakeServerInvalidationService::GetIdentityProvider() {
  return &identity_provider_;
}

void FakeServerInvalidationService::EnableSelfNotifications() {
  self_notify_ = true;
}

void FakeServerInvalidationService::DisableSelfNotifications() {
  self_notify_ = false;
}

void FakeServerInvalidationService::OnCommit(
    const std::string& committer_id,
    syncer::ModelTypeSet committed_model_types) {
  syncer::ObjectIdSet object_ids = syncer::ModelTypeSetToObjectIdSet(
      committed_model_types);
  syncer::ObjectIdInvalidationMap invalidation_map;
  for (syncer::ObjectIdSet::const_iterator it = object_ids.begin();
       it != object_ids.end(); ++it) {
    // TODO(pvalenzuela): Create more refined invalidations instead of
    // invalidating all items of a given type.

    if (self_notify_ || client_id_ != committer_id) {
      invalidation_map.Insert(syncer::Invalidation::InitUnknownVersion(*it));
    }
  }
  invalidator_registrar_.DispatchInvalidationsToHandlers(invalidation_map);
}

}  // namespace fake_server
