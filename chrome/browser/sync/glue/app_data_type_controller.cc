// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/app_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"

namespace browser_sync {

AppDataTypeController::AppDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
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
  ProfileSyncComponentsFactory::SyncComponents sync_components =
      profile_sync_factory_->CreateAppSyncComponents(sync_service_,
                                                     this);
  set_model_associator(sync_components.model_associator);
  generic_change_processor_.reset(static_cast<GenericChangeProcessor*>(
      sync_components.change_processor));
}

GenericChangeProcessor* AppDataTypeController::change_processor() const {
  return generic_change_processor_.get();
}

}  // namespace browser_sync
