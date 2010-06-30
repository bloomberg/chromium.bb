// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/registration_manager.h"

#include <string>

#include "chrome/browser/sync/notifier/invalidation_util.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "google/cacheinvalidation/invalidation-client.h"

namespace sync_notifier {

RegistrationManager::RegistrationManager(
    invalidation::InvalidationClient* invalidation_client)
    : invalidation_client_(invalidation_client) {
  DCHECK(invalidation_client_);
}

RegistrationManager::~RegistrationManager() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

bool RegistrationManager::RegisterType(syncable::ModelType model_type) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  invalidation::ObjectId object_id;
  if (!RealModelTypeToObjectId(model_type, &object_id)) {
    LOG(ERROR) << "Invalid model type: " << model_type;
    return false;
  }
  RegistrationStatusMap::iterator it =
      registration_status_.insert(
          std::make_pair(model_type, UNREGISTERED)).first;
  if (it->second == UNREGISTERED) {
    RegisterObject(object_id, it);
  }
  return true;
}

bool RegistrationManager::IsRegistered(
    syncable::ModelType model_type) const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  RegistrationStatusMap::const_iterator it =
      registration_status_.find(model_type);
  if (it == registration_status_.end()) {
    return false;
  }
  return it->second == REGISTERED;
}

void RegistrationManager::MarkRegistrationLost(
    syncable::ModelType model_type) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  invalidation::ObjectId object_id;
  if (!RealModelTypeToObjectId(model_type, &object_id)) {
    LOG(ERROR) << "Invalid model type: " << model_type;
    return;
  }
  RegistrationStatusMap::iterator it =
      registration_status_.find(model_type);
  if (it == registration_status_.end()) {
    LOG(ERROR) << "Unknown model type: "
               << syncable::ModelTypeToString(model_type);
    return;
  }
  it->second = UNREGISTERED;
  RegisterObject(object_id, it);
}

void RegistrationManager::MarkAllRegistrationsLost() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  for (RegistrationStatusMap::iterator it =
           registration_status_.begin();
       it != registration_status_.end(); ++it) {
    invalidation::ObjectId object_id;
    if (!RealModelTypeToObjectId(it->first, &object_id)) {
      LOG(DFATAL) << "Invalid model type: " << it->first;
      continue;
    }
    it->second = UNREGISTERED;
    RegisterObject(object_id, it);
  }
}

void RegistrationManager::RegisterObject(
    const invalidation::ObjectId& object_id,
    RegistrationStatusMap::iterator it) {
  DCHECK_EQ(it->second, UNREGISTERED);
  invalidation_client_->Register(
      object_id,
      invalidation::NewPermanentCallback(
          this, &RegistrationManager::OnRegister));
  it->second = PENDING;
}

void RegistrationManager::OnRegister(
    const invalidation::RegistrationUpdateResult& result) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  LOG(INFO) << "OnRegister: " << RegistrationUpdateResultToString(result);
  if (result.operation().type() !=
      invalidation::RegistrationUpdate::REGISTER) {
    LOG(ERROR) << "Got non-register result";
    return;
  }
  syncable::ModelType model_type;
  if (!ObjectIdToRealModelType(result.operation().object_id(),
                               &model_type)) {
    LOG(ERROR) << "Could not get model type";
    return;
  }
  RegistrationStatusMap::iterator it =
      registration_status_.find(model_type);
  if (it == registration_status_.end()) {
    LOG(ERROR) << "Unknown model type: " << model_type;
    return;
  }
  invalidation::Status::Code code = result.status().code();
  switch (code) {
    case invalidation::Status::SUCCESS:
      it->second = REGISTERED;
      break;
    case invalidation::Status::TRANSIENT_FAILURE:
      // TODO(akalin): Add retry logic.  For now, just fall through.
    default:
      // Treat everything else as a permanent failure.
      LOG(ERROR) << "Registration failed with code: " << code;
      break;
  }
}

}  // namespace sync_notifier
