// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_setting_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
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
    : NonFrontendDataTypeController(profile_sync_factory,
                                    profile,
                                    profile_sync_service),
      type_(type),
      profile_(profile),
      profile_sync_service_(profile_sync_service) {
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

bool ExtensionSettingDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return BrowserThread::PostTask(BrowserThread::FILE, from_here, task);
}

bool ExtensionSettingDataTypeController::StartModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_->InitExtensions(true);
  return true;
}

void ExtensionSettingDataTypeController::CreateSyncComponents() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK_EQ(state(), ASSOCIATING);
  ProfileSyncComponentsFactory::SyncComponents sync_components =
      profile_sync_factory()->CreateExtensionOrAppSettingSyncComponents(
          type_, profile_sync_service_, this);
  set_model_associator(sync_components.model_associator);
  set_change_processor(sync_components.change_processor);
}

}  // namespace browser_sync
