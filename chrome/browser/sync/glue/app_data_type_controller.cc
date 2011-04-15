// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/app_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_factory.h"

namespace browser_sync {

AppDataTypeController::AppDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : FrontendDataTypeController(profile_sync_factory,
                                 profile,
                                 sync_service) {
}

AppDataTypeController::~AppDataTypeController() {
}

syncable::ModelType AppDataTypeController::type() const {
  return syncable::APPS;
}

bool AppDataTypeController::StartModels() {
  profile_->InitExtensions(true);
  return true;
}

void AppDataTypeController::CreateSyncComponents() {
  ProfileSyncFactory::SyncComponents sync_components =
      profile_sync_factory_->CreateAppSyncComponents(sync_service_,
                                                     this);
  model_associator_.reset(sync_components.model_associator);
  change_processor_.reset(sync_components.change_processor);
}

void AppDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  UMA_HISTOGRAM_COUNTS("Sync.AppRunFailures", 1);
}

void AppDataTypeController::RecordAssociationTime(base::TimeDelta time) {
  UMA_HISTOGRAM_TIMES("Sync.AppAssociationTime", time);
}

void AppDataTypeController::RecordStartFailure(StartResult result) {
  UMA_HISTOGRAM_ENUMERATION("Sync.AppStartFailures",
                            result,
                            MAX_START_RESULT);
}

}  // namespace browser_sync
