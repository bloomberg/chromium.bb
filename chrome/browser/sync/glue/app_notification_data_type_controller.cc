// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/app_notification_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/app_notification_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/generic_change_processor.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "sync/api/syncable_service.h"

using content::BrowserThread;

namespace browser_sync {

AppNotificationDataTypeController::AppNotificationDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : UIDataTypeController(syncable::APP_NOTIFICATIONS,
                           profile_sync_factory,
                           profile,
                           sync_service) {
}

void AppNotificationDataTypeController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(chrome::NOTIFICATION_APP_NOTIFICATION_MANAGER_LOADED, type);
  registrar_.RemoveAll();
  DCHECK_EQ(state_, MODEL_STARTING);
  state_ = ASSOCIATING;
  Associate();
}

AppNotificationDataTypeController::~AppNotificationDataTypeController() {
}

AppNotificationManager*
AppNotificationDataTypeController::GetAppNotificationManager() {
  return profile_->GetExtensionService()->app_notification_manager();
}

// We want to start the AppNotificationManager before we begin associating.
bool AppNotificationDataTypeController::StartModels() {
  AppNotificationManager* manager = GetAppNotificationManager();
  DCHECK(manager);
  if (manager->loaded())
    return true;  // Continue to Associate().

  // Add an observer and continue when the AppNotificationManager is loaded.
  registrar_.Add(this, chrome::NOTIFICATION_APP_NOTIFICATION_MANAGER_LOADED,
                 content::Source<AppNotificationManager>(manager));
  return false;  // Don't continue Associate().
}

// Cleanup for our extra registrar usage.
void AppNotificationDataTypeController::StopModels() {
  registrar_.RemoveAll();
}

}  // namespace browser_sync
