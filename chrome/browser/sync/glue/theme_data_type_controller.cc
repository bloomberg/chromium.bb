// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/theme_data_type_controller.h"

#include "chrome/browser/extensions/extension_system.h"

namespace browser_sync {

ThemeDataTypeController::ThemeDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : UIDataTypeController(syncer::THEMES,
                           profile_sync_factory,
                           profile,
                           sync_service) {
}

ThemeDataTypeController::~ThemeDataTypeController() {}

bool ThemeDataTypeController::StartModels() {
  extensions::ExtensionSystem::Get(profile_)
      ->InitForRegularProfile(true, false);
  return true;
}

}  // namespace browser_sync
