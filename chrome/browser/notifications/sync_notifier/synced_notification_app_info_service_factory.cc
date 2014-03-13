// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification_app_info_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace notifier {

// static
SyncedNotificationAppInfoService*
SyncedNotificationAppInfoServiceFactory::GetForProfile(
    Profile* profile,
    Profile::ServiceAccessType service_access_type) {
  return static_cast<SyncedNotificationAppInfoService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SyncedNotificationAppInfoServiceFactory*
SyncedNotificationAppInfoServiceFactory::GetInstance() {
  return Singleton<SyncedNotificationAppInfoServiceFactory>::get();
}

SyncedNotificationAppInfoServiceFactory::
    SyncedNotificationAppInfoServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SyncedNotificationAppInfoService",
          BrowserContextDependencyManager::GetInstance()) {}

SyncedNotificationAppInfoServiceFactory::
    ~SyncedNotificationAppInfoServiceFactory() {}

KeyedService* SyncedNotificationAppInfoServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  SyncedNotificationAppInfoService* app_info_service =
      new SyncedNotificationAppInfoService(static_cast<Profile*>(profile));
  return app_info_service;
}

}  // namespace notifier
