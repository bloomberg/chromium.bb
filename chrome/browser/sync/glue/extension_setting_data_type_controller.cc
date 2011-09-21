// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_setting_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_settings_ui_wrapper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/api/syncable_service.h"
#include "chrome/browser/sync/glue/generic_change_processor.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "content/browser/browser_thread.h"

namespace browser_sync {

ExtensionSettingDataTypeController::ExtensionSettingDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* profile_sync_service)
    : NonFrontendDataTypeController(profile_sync_factory, profile),
      extension_settings_ui_wrapper_(
          profile->GetExtensionService()->extension_settings()),
      profile_sync_service_(profile_sync_service),
      extension_settings_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ExtensionSettingDataTypeController::~ExtensionSettingDataTypeController() {}

syncable::ModelType ExtensionSettingDataTypeController::type() const {
  return syncable::EXTENSION_SETTINGS;
}

browser_sync::ModelSafeGroup
ExtensionSettingDataTypeController::model_safe_group() const {
  return browser_sync::GROUP_FILE;
}

bool ExtensionSettingDataTypeController::StartModels() {
  return true;
}

bool ExtensionSettingDataTypeController::StartAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), ASSOCIATING);
  extension_settings_ui_wrapper_->RunWithSettings(
      base::Bind(
          &ExtensionSettingDataTypeController::
              StartAssociationWithExtensionSettings,
          this));
  return true;
}

void ExtensionSettingDataTypeController::StartAssociationWithExtensionSettings(
    ExtensionSettings* extension_settings) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  extension_settings_ = extension_settings;
  // Calls CreateSyncComponents, which expects extension_settings_ to be
  // non-NULL.
  StartAssociation();
}

void ExtensionSettingDataTypeController::CreateSyncComponents() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK_EQ(state(), ASSOCIATING);
  DCHECK(extension_settings_);
  ProfileSyncFactory::SyncComponents sync_components =
      profile_sync_factory()->CreateExtensionSettingSyncComponents(
          extension_settings_, profile_sync_service_, this);
  set_model_associator(sync_components.model_associator);
  set_change_processor(sync_components.change_processor);
}

bool ExtensionSettingDataTypeController::StopAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), STOPPING);
  if (!BrowserThread::PostTask(
          BrowserThread::FILE,
          FROM_HERE,
          base::Bind(
              &ExtensionSettingDataTypeController::StopAssociation,
              this))) {
    NOTREACHED();
  }
  return true;
}

void ExtensionSettingDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  UMA_HISTOGRAM_COUNTS("Sync.ExtensionSettingRunFailures", 1);
}

void ExtensionSettingDataTypeController::RecordAssociationTime(
    base::TimeDelta time) {
  UMA_HISTOGRAM_TIMES("Sync.ExtensionSettingAssociationTime", time);
}

void ExtensionSettingDataTypeController::RecordStartFailure(
    StartResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "Sync.ExtensionSettingStartFailures", result, MAX_START_RESULT);
}

}  // namespace browser_sync
