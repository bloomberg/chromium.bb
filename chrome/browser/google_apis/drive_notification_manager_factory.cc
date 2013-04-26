// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_notification_manager_factory.h"

#include "chrome/browser/google_apis/drive_notification_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"

namespace google_apis {

// static
DriveNotificationManager*
DriveNotificationManagerFactory::GetForProfile(Profile* profile) {
  if (!ProfileSyncService::IsSyncEnabled())
    return NULL;

  return static_cast<DriveNotificationManager*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
DriveNotificationManagerFactory*
DriveNotificationManagerFactory::GetInstance() {
  return Singleton<DriveNotificationManagerFactory>::get();
}

DriveNotificationManagerFactory::DriveNotificationManagerFactory()
    : ProfileKeyedServiceFactory("DriveNotificationManager",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

DriveNotificationManagerFactory::~DriveNotificationManagerFactory() {}

ProfileKeyedService* DriveNotificationManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new DriveNotificationManager(static_cast<Profile*>(profile));
}

}  // namespace google_apis
