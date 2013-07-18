// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/fake_invalidation_service.h"

#include "chrome/browser/invalidation/invalidation_service_util.h"

namespace invalidation {

FakeInvalidationService::FakeInvalidationService()
    : client_id_(GenerateInvalidatorClientId()) {
}

FakeInvalidationService::~FakeInvalidationService() {
}

void FakeInvalidationService::RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) {
  invalidator_registrar_.RegisterHandler(handler);
}

void FakeInvalidationService::UpdateRegisteredInvalidationIds(
      syncer::InvalidationHandler* handler,
      const syncer::ObjectIdSet& ids) {
  invalidator_registrar_.UpdateRegisteredIds(handler, ids);
}

void FakeInvalidationService::UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) {
  invalidator_registrar_.UnregisterHandler(handler);
}

void FakeInvalidationService::AcknowledgeInvalidation(
      const invalidation::ObjectId& id,
      const syncer::AckHandle& ack_handle) {
  // TODO(sync): Use assertions to ensure this function is invoked correctly.
}

syncer::InvalidatorState FakeInvalidationService::GetInvalidatorState() const {
  return syncer::INVALIDATIONS_ENABLED;
}

std::string FakeInvalidationService::GetInvalidatorClientId() const {
  return client_id_;
}

void FakeInvalidationService::EmitInvalidationForTest(
      const invalidation::ObjectId& object_id,
      int64 version,
      const std::string& payload) {
  syncer::ObjectIdInvalidationMap invalidation_map;

  syncer::Invalidation inv;
  inv.version = version;
  inv.payload = payload;
  inv.ack_handle = syncer::AckHandle::CreateUnique();

  invalidation_map.insert(std::make_pair(object_id, inv));

  invalidator_registrar_.DispatchInvalidationsToHandlers(invalidation_map);
}

}  // namespace invalidation
