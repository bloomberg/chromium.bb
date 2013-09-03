// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/drive_notification_manager_factory.h"

#include "chrome/browser/drive/drive_notification_manager.h"
#include "chrome/browser/invalidation/invalidation_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace drive {

// static
DriveNotificationManager*
DriveNotificationManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  if (!ProfileSyncService::IsSyncEnabled())
    return NULL;

  return static_cast<DriveNotificationManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
DriveNotificationManagerFactory*
DriveNotificationManagerFactory::GetInstance() {
  return Singleton<DriveNotificationManagerFactory>::get();
}

DriveNotificationManagerFactory::DriveNotificationManagerFactory()
    : BrowserContextKeyedServiceFactory(
        "DriveNotificationManager",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(invalidation::InvalidationServiceFactory::GetInstance());
}

DriveNotificationManagerFactory::~DriveNotificationManagerFactory() {}

BrowserContextKeyedService*
DriveNotificationManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DriveNotificationManager(
      invalidation::InvalidationServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context)));
}

}  // namespace drive
