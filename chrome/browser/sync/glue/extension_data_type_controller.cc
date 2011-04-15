// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_factory.h"

namespace browser_sync {

ExtensionDataTypeController::ExtensionDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : FrontendDataTypeController(profile_sync_factory,
                                 profile,
                                 sync_service) {
}

ExtensionDataTypeController::~ExtensionDataTypeController() {
}

syncable::ModelType ExtensionDataTypeController::type() const {
  return syncable::EXTENSIONS;
}

bool ExtensionDataTypeController::StartModels() {
  profile_->InitExtensions(true);
  return true;
}

void ExtensionDataTypeController::CreateSyncComponents() {
  ProfileSyncFactory::SyncComponents sync_components =
      profile_sync_factory_->CreateExtensionSyncComponents(sync_service_,
                                                     this);
  model_associator_.reset(sync_components.model_associator);
  change_processor_.reset(sync_components.change_processor);
}

void ExtensionDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  UMA_HISTOGRAM_COUNTS("Sync.ExtensionRunFailures", 1);
}

void ExtensionDataTypeController::RecordAssociationTime(base::TimeDelta time) {
  UMA_HISTOGRAM_TIMES("Sync.ExtensionAssociationTime", time);
}

void ExtensionDataTypeController::RecordStartFailure(StartResult result) {
  UMA_HISTOGRAM_ENUMERATION("Sync.ExtensionStartFailures",
                            result,
                            MAX_START_RESULT);
}

}  // namespace browser_sync
