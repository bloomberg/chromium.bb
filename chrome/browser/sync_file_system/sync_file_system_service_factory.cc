// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"

#include "base/command_line.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace sync_file_system {

namespace {
const char kDisableLastWriteWin[] = "disable-syncfs-last-write-win";
}

// static
SyncFileSystemService* SyncFileSystemServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SyncFileSystemService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SyncFileSystemServiceFactory* SyncFileSystemServiceFactory::GetInstance() {
  return Singleton<SyncFileSystemServiceFactory>::get();
}

void SyncFileSystemServiceFactory::set_mock_remote_file_service(
    scoped_ptr<RemoteFileSyncService> mock_remote_service) {
  mock_remote_file_service_ = mock_remote_service.Pass();
}

SyncFileSystemServiceFactory::SyncFileSystemServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "SyncFileSystemService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(drive::DriveNotificationManagerFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
}

SyncFileSystemServiceFactory::~SyncFileSystemServiceFactory() {}

BrowserContextKeyedService*
SyncFileSystemServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  SyncFileSystemService* service = new SyncFileSystemService(profile);

  scoped_ptr<LocalFileSyncService> local_file_service(
      new LocalFileSyncService(profile));

  scoped_ptr<RemoteFileSyncService> remote_file_service;
  if (mock_remote_file_service_) {
    remote_file_service = mock_remote_file_service_.Pass();
  } else {
    // FileSystem needs to be registered before DriveFileSyncService runs
    // its initialization code.
    RegisterSyncableFileSystem();
    remote_file_service =
        DriveFileSyncService::Create(profile).PassAs<RemoteFileSyncService>();
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(kDisableLastWriteWin)) {
    remote_file_service->SetConflictResolutionPolicy(
        CONFLICT_RESOLUTION_POLICY_MANUAL);
  }

  service->Initialize(local_file_service.Pass(),
                      remote_file_service.Pass());
  return service;
}

}  // namespace sync_file_system
