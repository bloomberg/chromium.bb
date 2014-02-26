// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/data_type_manager.h"
#include "components/sync_driver/failed_data_types_handler.h"

using browser_sync::DataTypeManager;

namespace browser_sync {

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

  for (TypeErrorMap::const_iterator iter = errors.begin(); iter != errors.end();
       ++iter) {
    syncer::SyncError::ErrorType failure_type = iter->second.error_type();
    switch (failure_type) {
      case syncer::SyncError::UNRECOVERABLE_ERROR:
      case syncer::SyncError::DATATYPE_ERROR:
        fatal_errors_.insert(*iter);
        break;
      case syncer::SyncError::CRYPTO_ERROR:
        crypto_errors_.insert(*iter);
        break;
      case syncer::SyncError::PERSISTENCE_ERROR:
        persistence_errors_.insert(*iter);
        break;
      default:
        NOTREACHED();
    }
  }
  return true;
}

void FailedDataTypesHandler::Reset() {
  fatal_errors_.clear();
  crypto_errors_.clear();
  persistence_errors_.clear();
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

FailedDataTypesHandler::TypeErrorMap FailedDataTypesHandler::GetAllErrors()
    const {
  TypeErrorMap result;

  if (AnyFailedDataType()) {
    result = fatal_errors_;
    result.insert(crypto_errors_.begin(), crypto_errors_.end());
    result.insert(persistence_errors_.begin(), persistence_errors_.end());
  }
  return result;
}

syncer::ModelTypeSet FailedDataTypesHandler::GetFailedTypes() const {
  syncer::ModelTypeSet result = GetFatalErrorTypes();
  result.PutAll(GetCryptoErrorTypes());
  return result;
}

syncer::ModelTypeSet FailedDataTypesHandler::GetFatalErrorTypes() const {
  return GetTypesFromErrorMap(fatal_errors_);
}

syncer::ModelTypeSet FailedDataTypesHandler::GetCryptoErrorTypes() const {
  syncer::ModelTypeSet result = GetTypesFromErrorMap(crypto_errors_);
  return result;
}

syncer::ModelTypeSet FailedDataTypesHandler::GetPersistenceErrorTypes() const {
  syncer::ModelTypeSet result = GetTypesFromErrorMap(persistence_errors_);
  return result;
}

bool FailedDataTypesHandler::AnyFailedDataType() const {
  // Note: persistence errors are not failed types. They just trigger automatic
  // unapply + getupdates, at which point they are associated like normal.
  return !fatal_errors_.empty() || !crypto_errors_.empty();
}

}  // namespace browser_sync
