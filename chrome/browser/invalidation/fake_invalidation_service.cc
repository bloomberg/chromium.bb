// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/fake_invalidation_service.h"

#include "chrome/browser/invalidation/invalidation_service_util.h"

namespace invalidation {

FakeInvalidationService::FakeInvalidationService()
    : client_id_(GenerateInvalidatorClientId()),
      received_invalid_acknowledgement_(false) {
  invalidator_registrar_.UpdateInvalidatorState(syncer::INVALIDATIONS_ENABLED);
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
  // Try to find the given handle and object id in the unacknowledged list.
  AckHandleList::iterator handle;
  AckHandleList::iterator begin = unacknowledged_handles_.begin();
  AckHandleList::iterator end = unacknowledged_handles_.end();
  for (handle = begin; handle != end; ++handle)
    if (handle->first.Equals(ack_handle) && handle->second == id)
      break;
  if (handle == end)
    received_invalid_acknowledgement_ = false;
  else
    unacknowledged_handles_.erase(handle);

  // Add to the acknowledged list.
  acknowledged_handles_.push_back(AckHandleList::value_type(ack_handle, id));
}

syncer::InvalidatorState FakeInvalidationService::GetInvalidatorState() const {
  return invalidator_registrar_.GetInvalidatorState();
}

std::string FakeInvalidationService::GetInvalidatorClientId() const {
  return client_id_;
}

void FakeInvalidationService::SetInvalidatorState(
    syncer::InvalidatorState state) {
  invalidator_registrar_.UpdateInvalidatorState(state);
}

void FakeInvalidationService::EmitInvalidationForTest(
    const syncer::Invalidation& invalidation) {
  syncer::ObjectIdInvalidationMap invalidation_map;

  invalidation_map.Insert(invalidation);
  unacknowledged_handles_.push_back(
      AckHandleList::value_type(
          invalidation.ack_handle(),
          invalidation.object_id()));

  invalidator_registrar_.DispatchInvalidationsToHandlers(invalidation_map);
}

bool FakeInvalidationService::IsInvalidationAcknowledged(
    const syncer::AckHandle& ack_handle) const {
  // Try to find the given handle in the acknowledged list.
  AckHandleList::const_iterator begin = acknowledged_handles_.begin();
  AckHandleList::const_iterator end = acknowledged_handles_.end();
  for (AckHandleList::const_iterator handle = begin; handle != end; ++handle)
    if (handle->first.Equals(ack_handle))
      return true;
  return false;
}

}  // namespace invalidation
