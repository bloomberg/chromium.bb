// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/preference_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/api/syncable_service.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"

namespace browser_sync {

PreferenceDataTypeController::PreferenceDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : FrontendDataTypeController(profile_sync_factory,
                                 profile,
                                 sync_service) {
}

PreferenceDataTypeController::~PreferenceDataTypeController() {
}

syncable::ModelType PreferenceDataTypeController::type() const {
  return syncable::PREFERENCES;
}

void PreferenceDataTypeController::CreateSyncComponents() {
  ProfileSyncComponentsFactory::SyncComponents sync_components =
      profile_sync_factory_->CreatePreferenceSyncComponents(sync_service_,
                                                            this);
  set_model_associator(sync_components.model_associator);
  generic_change_processor_.reset(static_cast<GenericChangeProcessor*>(
      sync_components.change_processor));
}

GenericChangeProcessor* PreferenceDataTypeController::change_processor() const {
  return generic_change_processor_.get();
}

}  // namespace browser_sync
