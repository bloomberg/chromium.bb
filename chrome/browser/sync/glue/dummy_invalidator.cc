// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/dummy_invalidator.h"

DummyInvalidator::DummyInvalidator() {}
DummyInvalidator::~DummyInvalidator() {}

void DummyInvalidator::RegisterHandler(syncer::InvalidationHandler* handler) {}

void DummyInvalidator::UpdateRegisteredIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {}

void DummyInvalidator::UnregisterHandler(
    syncer::InvalidationHandler* handler) {}

void DummyInvalidator::Acknowledge(const invalidation::ObjectId& id,
                                   const syncer::AckHandle& ack_handle) {}

syncer::InvalidatorState DummyInvalidator::GetInvalidatorState() const {
  return syncer::TRANSIENT_INVALIDATION_ERROR;
}

void DummyInvalidator::UpdateCredentials(
    const std::string& email, const std::string& token) {}

void DummyInvalidator::SendInvalidation(
    const syncer::ObjectIdInvalidationMap& invalidation_map) {}
