// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"

namespace browser_sync {

ExtensionDataTypeController::ExtensionDataTypeController(
    syncable::ModelType type,
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : UIDataTypeController(type,
                           profile_sync_factory,
                           profile,
                           sync_service) {
  DCHECK(type == syncable::EXTENSIONS ||
         type == syncable::APPS);
}

ExtensionDataTypeController::~ExtensionDataTypeController() {}

bool ExtensionDataTypeController::StartModels() {
  ExtensionSystem::Get(profile_)->Init(true);
  return true;
}

}  // namespace browser_sync
