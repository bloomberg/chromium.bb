// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"

namespace browser_sync {

ExtensionDataTypeController::ExtensionDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
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
  ProfileSyncComponentsFactory::SyncComponents sync_components =
      profile_sync_factory_->CreateExtensionSyncComponents(sync_service_,
                                                     this);
  set_model_associator(sync_components.model_associator);
  generic_change_processor_.reset(static_cast<GenericChangeProcessor*>(
      sync_components.change_processor));
}

GenericChangeProcessor* ExtensionDataTypeController::change_processor() const {
  return generic_change_processor_.get();
}

}  // namespace browser_sync
