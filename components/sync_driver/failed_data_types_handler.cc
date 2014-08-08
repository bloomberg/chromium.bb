// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/data_type_manager.h"
#include "components/sync_driver/failed_data_types_handler.h"

namespace sync_driver {

namespace {

syncer::ModelTypeSet GetTypesFromErrorMap(
    const FailedDataTypesHandler::TypeErrorMap& errors) {
  syncer::ModelTypeSet result;
  for (FailedDataTypesHandler::TypeErrorMap::const_iterator it = errors.begin();
       it != errors.end(); ++it) {
    DCHECK(!result.Has(it->first));
    result.Put(it->first);
  }
  return result;
}

}  // namespace

FailedDataTypesHandler::FailedDataTypesHandler() {
}

FailedDataTypesHandler::~FailedDataTypesHandler() {
}

bool FailedDataTypesHandler::UpdateFailedDataTypes(const TypeErrorMap& errors) {
  if (errors.empty())
    return false;

  DVLOG(1) << "Setting " << errors.size() << " new failed types.";

  for (TypeErrorMap::const_iterator iter = errors.begin(); iter != errors.end();
       ++iter) {
    syncer::SyncError::ErrorType failure_type = iter->second.error_type();
    switch (failure_type) {
      case syncer::SyncError::UNSET:
        NOTREACHED();
        break;
      case syncer::SyncError::UNRECOVERABLE_ERROR:
        unrecoverable_errors_.insert(*iter);
        break;
      case syncer::SyncError::DATATYPE_ERROR:
      case syncer::SyncError::DATATYPE_POLICY_ERROR:
        data_type_errors_.insert(*iter);
        break;
      case syncer::SyncError::CRYPTO_ERROR:
        crypto_errors_.insert(*iter);
        break;
      case syncer::SyncError::PERSISTENCE_ERROR:
        persistence_errors_.insert(*iter);
        break;
      case syncer::SyncError::UNREADY_ERROR:
        unready_errors_.insert(*iter);
        break;
    }
  }
  return true;
}

void FailedDataTypesHandler::Reset() {
  unrecoverable_errors_.clear();
  data_type_errors_.clear();
  crypto_errors_.clear();
  persistence_errors_.clear();
  unready_errors_.clear();
}

void FailedDataTypesHandler::ResetCryptoErrors() {
  crypto_errors_.clear();
}

void FailedDataTypesHandler::ResetPersistenceErrorsFrom(
    syncer::ModelTypeSet purged_types) {
  for (syncer::ModelTypeSet::Iterator iter = purged_types.First(); iter.Good();
       iter.Inc()) {
    persistence_errors_.erase(iter.Get());
  }
}

bool FailedDataTypesHandler::ResetDataTypeErrorFor(syncer::ModelType type) {
  return data_type_errors_.erase(type) > 0;
}

bool FailedDataTypesHandler::ResetUnreadyErrorFor(syncer::ModelType type) {
  return unready_errors_.erase(type) > 0;
}

FailedDataTypesHandler::TypeErrorMap FailedDataTypesHandler::GetAllErrors()
    const {
  TypeErrorMap result;
  result.insert(data_type_errors_.begin(), data_type_errors_.end());
  result.insert(crypto_errors_.begin(), crypto_errors_.end());
  result.insert(persistence_errors_.begin(), persistence_errors_.end());
  result.insert(unready_errors_.begin(), unready_errors_.end());
  result.insert(unrecoverable_errors_.begin(), unrecoverable_errors_.end());
  return result;
}

syncer::ModelTypeSet FailedDataTypesHandler::GetFailedTypes() const {
  syncer::ModelTypeSet result = GetFatalErrorTypes();
  result.PutAll(GetCryptoErrorTypes());
  result.PutAll(GetUnreadyErrorTypes());
  return result;
}

syncer::ModelTypeSet FailedDataTypesHandler::GetFatalErrorTypes()
    const {
  syncer::ModelTypeSet result;
  result.PutAll(GetTypesFromErrorMap(data_type_errors_));
  result.PutAll(GetTypesFromErrorMap(unrecoverable_errors_));
  return result;
}

syncer::ModelTypeSet FailedDataTypesHandler::GetCryptoErrorTypes() const {
  syncer::ModelTypeSet result = GetTypesFromErrorMap(crypto_errors_);
  return result;
}

syncer::ModelTypeSet FailedDataTypesHandler::GetPersistenceErrorTypes() const {
  syncer::ModelTypeSet result = GetTypesFromErrorMap(persistence_errors_);
  return result;
}

syncer::ModelTypeSet FailedDataTypesHandler::GetUnreadyErrorTypes() const {
  syncer::ModelTypeSet result = GetTypesFromErrorMap(unready_errors_);
  return result;
}

syncer::ModelTypeSet FailedDataTypesHandler::GetUnrecoverableErrorTypes()
    const {
  syncer::ModelTypeSet result = GetTypesFromErrorMap(unrecoverable_errors_);
  return result;
}

syncer::SyncError FailedDataTypesHandler::GetUnrecoverableError() const {
  // Just return the first one. It is assumed all the unrecoverable errors
  // have the same cause. The others are just tracked to know which types
  // were involved.
  return (unrecoverable_errors_.empty()
              ? syncer::SyncError()
              : unrecoverable_errors_.begin()->second);
}

bool FailedDataTypesHandler::AnyFailedDataType() const {
  // Note: persistence errors are not failed types. They just trigger automatic
  // unapply + getupdates, at which point they are associated like normal.
  return unrecoverable_errors_.empty() ||
         !data_type_errors_.empty() ||
         !crypto_errors_.empty();
}

}  // namespace sync_driver
