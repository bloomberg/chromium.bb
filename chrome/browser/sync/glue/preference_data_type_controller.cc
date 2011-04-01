// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/preference_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/sync/profile_sync_factory.h"

namespace browser_sync {

PreferenceDataTypeController::PreferenceDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : FrontendDataTypeController(profile_sync_factory,
                                 profile,
                                 sync_service) {
}

PreferenceDataTypeController::~PreferenceDataTypeController() {}

syncable::ModelType PreferenceDataTypeController::type() const {
  return syncable::PREFERENCES;
}

void PreferenceDataTypeController::CreateSyncComponents() {
  ProfileSyncFactory::SyncComponents sync_components = profile_sync_factory_->
      CreatePreferenceSyncComponents(sync_service_, this);
  model_associator_.reset(sync_components.model_associator);
  change_processor_.reset(sync_components.change_processor);
}

void PreferenceDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  UMA_HISTOGRAM_COUNTS("Sync.PreferenceRunFailures", 1);
}

void PreferenceDataTypeController::RecordAssociationTime(base::TimeDelta time) {
  UMA_HISTOGRAM_TIMES("Sync.PreferenceAssociationTime", time);
}

void PreferenceDataTypeController::RecordStartFailure(StartResult result) {
  UMA_HISTOGRAM_ENUMERATION("Sync.PreferenceStartFailures",
                            result,
                            MAX_START_RESULT);
}

}  // namespace browser_sync
