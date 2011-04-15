// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/theme_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_factory.h"

namespace browser_sync {

ThemeDataTypeController::ThemeDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : FrontendDataTypeController(profile_sync_factory,
                                 profile,
                                 sync_service) {
}

ThemeDataTypeController::~ThemeDataTypeController() {
}

syncable::ModelType ThemeDataTypeController::type() const {
  return syncable::THEMES;
}

bool ThemeDataTypeController::StartModels() {
  profile_->InitExtensions(true);
  return true;
}

void ThemeDataTypeController::CreateSyncComponents() {
  ProfileSyncFactory::SyncComponents sync_components =
      profile_sync_factory_->CreateThemeSyncComponents(sync_service_,
                                                     this);
  model_associator_.reset(sync_components.model_associator);
  change_processor_.reset(sync_components.change_processor);
}

void ThemeDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  UMA_HISTOGRAM_COUNTS("Sync.ThemeRunFailures", 1);
}

void ThemeDataTypeController::RecordAssociationTime(base::TimeDelta time) {
  UMA_HISTOGRAM_TIMES("Sync.ThemeAssociationTime", time);
}

void ThemeDataTypeController::RecordStartFailure(StartResult result) {
  UMA_HISTOGRAM_ENUMERATION("Sync.ThemeStartFailures",
                            result,
                            MAX_START_RESULT);
}

}  // namespace browser_sync
