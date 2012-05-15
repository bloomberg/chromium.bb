// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_setting_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/generic_change_processor.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/syncable_service.h"

using content::BrowserThread;

namespace browser_sync {

ExtensionSettingDataTypeController::ExtensionSettingDataTypeController(
    syncable::ModelType type,
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* profile_sync_service)
    : NewNonFrontendDataTypeController(profile_sync_factory,
                                       profile,
                                       profile_sync_service),
      type_(type),
      profile_(profile),
      profile_sync_service_(profile_sync_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(type == syncable::EXTENSION_SETTINGS ||
         type == syncable::APP_SETTINGS);
}

syncable::ModelType ExtensionSettingDataTypeController::type() const {
  return type_;
}

browser_sync::ModelSafeGroup
ExtensionSettingDataTypeController::model_safe_group() const {
  return browser_sync::GROUP_FILE;
}

ExtensionSettingDataTypeController::~ExtensionSettingDataTypeController() {}

bool ExtensionSettingDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return BrowserThread::PostTask(BrowserThread::FILE, from_here, task);
}

bool ExtensionSettingDataTypeController::StartModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ExtensionSystem::Get(profile_)->Init(true);
  return true;
}

}  // namespace browser_sync
