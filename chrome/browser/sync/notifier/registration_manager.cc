// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/registration_manager.h"

#include <string>

#include "chrome/browser/sync/notifier/invalidation_util.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace sync_notifier {

RegistrationManager::RegistrationManager(
    invalidation::InvalidationClient* invalidation_client)
    : invalidation_client_(invalidation_client) {
  DCHECK(invalidation_client_);
}

RegistrationManager::~RegistrationManager() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

void RegistrationManager::SetRegisteredTypes(
    const syncable::ModelTypeSet& types) {
  DCHECK(non_thread_safe_.CalledOnValidThread());

  for (int i = syncable::FIRST_REAL_MODEL_TYPE; i < syncable::MODEL_TYPE_COUNT;
       ++i) {
    syncable::ModelType type = syncable::ModelTypeFromInt(i);
    if (types.count(type) > 0) {
      RegisterType(type);
    } else {
      UnregisterType(type);
    }
  }
}

bool RegistrationManager::IsRegistered(
    syncable::ModelType model_type) const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  RegistrationStatusMap::const_iterator it =
      registration_status_.find(model_type);
  if (it == registration_status_.end()) {
    return false;
  }
  return it->second == invalidation::RegistrationState_REGISTERED;
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
    LOG(ERROR) << "Unknown model type: " << model_type;
    return;
  }
  it->second = invalidation::RegistrationState_UNREGISTERED;
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
    it->second = invalidation::RegistrationState_UNREGISTERED;
    RegisterObject(object_id, it);
  }
}

void RegistrationManager::RegisterType(syncable::ModelType model_type) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  invalidation::ObjectId object_id;
  if (!RealModelTypeToObjectId(model_type, &object_id)) {
    LOG(DFATAL) << "Invalid model type: " << model_type;
    return;
  }
  RegistrationStatusMap::iterator it =
      registration_status_.insert(
          std::make_pair(
              model_type,
              invalidation::RegistrationState_UNREGISTERED)).first;
  if (it->second == invalidation::RegistrationState_UNREGISTERED) {
    RegisterObject(object_id, it);
  }
}

void RegistrationManager::UnregisterType(syncable::ModelType model_type) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  invalidation::ObjectId object_id;
  if (!RealModelTypeToObjectId(model_type, &object_id)) {
    LOG(DFATAL) << "Invalid model type: " << model_type;
    return;
  }
  RegistrationStatusMap::iterator it = registration_status_.find(model_type);

  if (it != registration_status_.end()) {
    if (it->second == invalidation::RegistrationState_REGISTERED) {
      invalidation_client_->Unregister(object_id);
    }
    registration_status_.erase(it);
  }
}

void RegistrationManager::RegisterObject(
    const invalidation::ObjectId& object_id,
    RegistrationStatusMap::iterator it) {
  DCHECK_EQ(it->second, invalidation::RegistrationState_UNREGISTERED);
  invalidation_client_->Register(object_id);
  it->second = invalidation::RegistrationState_REGISTERED;
}
}  // namespace sync_notifier
