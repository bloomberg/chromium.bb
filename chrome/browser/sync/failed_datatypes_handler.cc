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
    const std::list<csync::SyncError>& errors) {
  syncable::ModelTypeSet result;
  for (std::list<csync::SyncError>::const_iterator it = errors.begin();
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
    const std::list<csync::SyncError>& errors,
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

std::string GetErrorStringFromErrors(
    const std::list<csync::SyncError>& errors) {
  std::string message;
  for (std::list<csync::SyncError>::const_iterator it = errors.begin();
       it != errors.end(); ++it) {
    if (it != errors.begin()) {
      message += ", ";
    }
    message += std::string(syncable::ModelTypeToString(it->type())) + " " +
        it->location().ToString() + ": " + it->message();
  }
  return message;

}

std::string FailedDatatypesHandler::GetErrorString() const {
  std::string message;

  if (!startup_errors_.empty()) {
    message = "Sync configuration failed when starting ";
    message += GetErrorStringFromErrors(startup_errors_);
    message += ". ";
  }

  if (!runtime_errors_.empty()) {
    message += "The following errors were encountered at runtime: ";
    message += GetErrorStringFromErrors(runtime_errors_);
    message += ".";
  }
  return message;
}

bool FailedDatatypesHandler::AnyFailedDatatype() const {
  return (!startup_errors_.empty() || !runtime_errors_.empty());
}

