// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/failed_datatypes_handler.h"
#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"

using browser_sync::DataTypeManager;

FailedDatatypesHandler::FailedDatatypesHandler(ProfileSyncService* service)
    : service_(service) {
}

FailedDatatypesHandler::~FailedDatatypesHandler() {
}

syncable::ModelTypeSet GetTypesFromErrorsList(
    const std::vector<syncer::SyncError>& errors) {
  syncable::ModelTypeSet result;
  for (std::vector<syncer::SyncError>::const_iterator it = errors.begin();
       it != errors.end(); ++it) {
    DCHECK(!result.Has(it->type()));
    result.Put(it->type());
  }
  return result;
}

syncable::ModelTypeSet FailedDatatypesHandler::GetFailedTypes() const {
  syncable::ModelTypeSet result = GetTypesFromErrorsList(startup_errors_);
  result.PutAll(GetTypesFromErrorsList(runtime_errors_));
  return result;
}

bool FailedDatatypesHandler::UpdateFailedDatatypes(
    const std::list<syncer::SyncError>& errors,
    FailureType failure_type) {
  const syncable::ModelTypeSet types = GetFailedTypes();
  if (failure_type == RUNTIME) {
    runtime_errors_.insert(runtime_errors_.end(),
                           errors.begin(),
                           errors.end());
  } else if (failure_type == STARTUP) {
    startup_errors_.insert(startup_errors_.end(),
                           errors.begin(),
                           errors.end());
  } else {
    NOTREACHED();
  }

  return !errors.empty();
}

void FailedDatatypesHandler::OnUserChoseDatatypes() {
  startup_errors_.clear();
  runtime_errors_.clear();
}

std::vector<syncer::SyncError> FailedDatatypesHandler::GetAllErrors() const {
  std::vector<syncer::SyncError> result;

  if (AnyFailedDatatype()) {
    result.insert(result.end(), startup_errors_.begin(), startup_errors_.end());
    result.insert(result.end(), runtime_errors_.begin(), runtime_errors_.end());
  }
  return result;
}

bool FailedDatatypesHandler::AnyFailedDatatype() const {
  return (!startup_errors_.empty() || !runtime_errors_.empty());
}

