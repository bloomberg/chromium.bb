// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/theme_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"

namespace browser_sync {

ThemeDataTypeController::ThemeDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : FrontendDataTypeController(profile_sync_factory,
                                 profile,
                                 sync_service) {
}

syncable::ModelType ThemeDataTypeController::type() const {
  return syncable::THEMES;
}

ThemeDataTypeController::~ThemeDataTypeController() {}

bool ThemeDataTypeController::StartModels() {
  ExtensionSystem::Get(profile_)->Init(true);
  return true;
}

void ThemeDataTypeController::CreateSyncComponents() {
  ProfileSyncComponentsFactory::SyncComponents sync_components =
      profile_sync_factory_->CreateThemeSyncComponents(sync_service_,
                                                       this);
  set_model_associator(sync_components.model_associator);
  set_change_processor(sync_components.change_processor);
}

}  // namespace browser_sync
