// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_setting_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "components/sync_driver/generic_change_processor.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "sync/api/syncable_service.h"

using content::BrowserThread;

namespace browser_sync {

ExtensionSettingDataTypeController::ExtensionSettingDataTypeController(
    syncer::ModelType type,
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    const DisableTypeCallback& disable_callback)
    : NonUIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          base::Bind(&ChromeReportUnrecoverableError),
          disable_callback,
          profile_sync_factory),
      type_(type),
      profile_(profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(type == syncer::EXTENSION_SETTINGS || type == syncer::APP_SETTINGS);
}

syncer::ModelType ExtensionSettingDataTypeController::type() const {
  return type_;
}

syncer::ModelSafeGroup
ExtensionSettingDataTypeController::model_safe_group() const {
  return syncer::GROUP_FILE;
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
  extensions::ExtensionSystem::Get(profile_)->InitForRegularProfile(true);
  return true;
}

}  // namespace browser_sync
