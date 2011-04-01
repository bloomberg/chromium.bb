// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/session_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/sync/profile_sync_factory.h"

namespace browser_sync {

SessionDataTypeController::SessionDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : FrontendDataTypeController(profile_sync_factory,
                                 profile,
                                 sync_service) {
}

SessionDataTypeController::~SessionDataTypeController() {}

SessionModelAssociator* SessionDataTypeController::GetModelAssociator() {
  return reinterpret_cast<SessionModelAssociator*>(model_associator_.get());
}

syncable::ModelType SessionDataTypeController::type() const {
  return syncable::SESSIONS;
}

void SessionDataTypeController::CreateSyncComponents() {
  ProfileSyncFactory::SyncComponents sync_components = profile_sync_factory_->
      CreateSessionSyncComponents(sync_service_, this);
  model_associator_.reset(sync_components.model_associator);
  change_processor_.reset(sync_components.change_processor);
}

void SessionDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  UMA_HISTOGRAM_COUNTS("Sync.SessionRunFailures", 1);
}

void SessionDataTypeController::RecordAssociationTime(base::TimeDelta time) {
  UMA_HISTOGRAM_TIMES("Sync.SessionAssociationTime", time);
}

void SessionDataTypeController::RecordStartFailure(StartResult result) {
  UMA_HISTOGRAM_ENUMERATION("Sync.SessionStartFailures",
                            result,
                            MAX_START_RESULT);
}

}  // namespace browser_sync

