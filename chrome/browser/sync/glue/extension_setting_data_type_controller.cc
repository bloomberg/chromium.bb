// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_setting_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings/settings_frontend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/api/syncable_service.h"
#include "chrome/browser/sync/glue/generic_change_processor.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace browser_sync {

ExtensionSettingDataTypeController::ExtensionSettingDataTypeController(
    syncable::ModelType type,
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* profile_sync_service)
    : NonFrontendDataTypeController(profile_sync_factory, profile),
      type_(type),
      settings_frontend_(
          profile->GetExtensionService()->settings_frontend()),
      profile_sync_service_(profile_sync_service),
      settings_service_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(type == syncable::EXTENSION_SETTINGS ||
         type == syncable::APP_SETTINGS);
}

ExtensionSettingDataTypeController::~ExtensionSettingDataTypeController() {}

syncable::ModelType ExtensionSettingDataTypeController::type() const {
  return type_;
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
  settings_frontend_->RunWithSyncableService(
      type_,
      base::Bind(
          &ExtensionSettingDataTypeController::
              StartAssociationWithExtensionSettingsService,
          this));
  return true;
}

void ExtensionSettingDataTypeController::
    StartAssociationWithExtensionSettingsService(
        SyncableService* settings_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  settings_service_ = settings_service;
  // Calls CreateSyncComponents, which expects settings_service_ to be
  // non-NULL.
  StartAssociation();
}

void ExtensionSettingDataTypeController::CreateSyncComponents() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK_EQ(state(), ASSOCIATING);
  DCHECK(settings_service_);
  ProfileSyncComponentsFactory::SyncComponents sync_components =
      profile_sync_factory()->CreateExtensionOrAppSettingSyncComponents(
          type_, settings_service_, profile_sync_service_, this);
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
