// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

syncable::ModelTypeSet FailedDatatypesHandler::GetFailedTypes() const {
  syncable::ModelTypeSet result;
  for (std::list<SyncError>::const_iterator it = errors_.begin();
       it != errors_.end(); ++it) {
    DCHECK(result.count(it->type()) == 0);
    result.insert(it->type());
  }
  return result;
}

bool FailedDatatypesHandler::UpdateFailedDatatypes(
    DataTypeManager::ConfigureResult result) {
  syncable::ModelTypeSet types = GetFailedTypes();
  bool any_new_failed_types = false;
  for (std::list<SyncError>::iterator it = result.errors.begin();
       it != result.errors.end(); ++it) {
    DCHECK(types.count(it->type()) == 0);
    any_new_failed_types = true;
    errors_.push_back(*it);
  }

  return any_new_failed_types;
}

void FailedDatatypesHandler::OnUserChoseDatatypes() {
  errors_.clear();
}

std::string FailedDatatypesHandler::GetErrorString() const {
  std::string message = "Sync configuration failed when starting ";
  for (std::list<SyncError>::const_iterator it = errors_.begin();
       it != errors_.end(); ++it) {
    if (it != errors_.begin()) {
      message += ", ";
    }
    message += std::string(syncable::ModelTypeToString(it->type())) + " " +
        it->location().ToString() + ": " + it->message();
  }
  return message;
}

bool FailedDatatypesHandler::AnyFailedDatatype() const {
  return (!errors_.empty());
}

