// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"

#include "base/command_line.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/drive/drive_api_service.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/google_apis/drive_api_url_generator.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace sync_file_system {

namespace {
const char kDisableLastWriteWin[] = "disable-syncfs-last-write-win";
const char kEnableSyncFileSystemV2[] = "enable-syncfs-v2";
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
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

SyncFileSystemServiceFactory::~SyncFileSystemServiceFactory() {}

BrowserContextKeyedService*
SyncFileSystemServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  SyncFileSystemService* service = new SyncFileSystemService(profile);

  scoped_ptr<LocalFileSyncService> local_file_service(
      new LocalFileSyncService(profile));

  scoped_ptr<RemoteFileSyncService> remote_file_service;
  if (mock_remote_file_service_) {
    remote_file_service = mock_remote_file_service_.Pass();
  } else if (CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableSyncFileSystemV2)) {
    GURL base_drive_url(
        google_apis::DriveApiUrlGenerator::kBaseUrlForProduction);
    GURL base_download_url(
        google_apis::DriveApiUrlGenerator::kBaseDownloadUrlForProduction);
    GURL wapi_base_url(
        google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction);

    scoped_refptr<base::SequencedWorkerPool> worker_pool(
        content::BrowserThread::GetBlockingPool());

    ProfileOAuth2TokenService* token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
    scoped_ptr<drive::DriveAPIService> drive_api_service(
        new drive::DriveAPIService(
            token_service,
            token_service->GetPrimaryAccountId(),
            context->GetRequestContext(),
            worker_pool.get(),
            base_drive_url, base_download_url, wapi_base_url,
            std::string() /* custom_user_agent */));

    drive::DriveNotificationManager* notification_manager =
        drive::DriveNotificationManagerFactory::GetForBrowserContext(profile);
    ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile)->extension_service();

    scoped_refptr<base::SequencedTaskRunner> task_runner(
        worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
            worker_pool->GetSequenceToken(),
            base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));

    scoped_ptr<drive_backend::SyncEngine> sync_engine(
        new drive_backend::SyncEngine(
            context->GetPath(),
            task_runner.get(),
            drive_api_service.Pass(),
            notification_manager,
            extension_service));

    sync_engine->Initialize();
    remote_file_service = sync_engine.PassAs<RemoteFileSyncService>();
  } else {
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
