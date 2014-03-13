// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"

#include <set>

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"
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
  typedef std::set<BrowserContextKeyedServiceFactory*> FactorySet;
  FactorySet factories;
  RemoteFileSyncService::AppendDependsOnFactories(
      RemoteFileSyncService::V1, &factories);
  RemoteFileSyncService::AppendDependsOnFactories(
      RemoteFileSyncService::V2, &factories);
  for (FactorySet::iterator iter = factories.begin();
       iter != factories.end();
       ++iter) {
    DependsOn(*iter);
  }
}

SyncFileSystemServiceFactory::~SyncFileSystemServiceFactory() {}

BrowserContextKeyedService*
SyncFileSystemServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  SyncFileSystemService* service = new SyncFileSystemService(profile);

  scoped_ptr<LocalFileSyncService> local_file_service =
      LocalFileSyncService::Create(profile);

  scoped_ptr<RemoteFileSyncService> remote_file_service;
  if (mock_remote_file_service_) {
    remote_file_service = mock_remote_file_service_.Pass();
  } else if (IsV2Enabled()) {
    remote_file_service = RemoteFileSyncService::CreateForBrowserContext(
        RemoteFileSyncService::V2, context);
  } else {
    remote_file_service = RemoteFileSyncService::CreateForBrowserContext(
        RemoteFileSyncService::V1, context);
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(kDisableLastWriteWin)) {
    remote_file_service->SetDefaultConflictResolutionPolicy(
        CONFLICT_RESOLUTION_POLICY_MANUAL);
  }

  service->Initialize(local_file_service.Pass(),
                      remote_file_service.Pass());
  return service;
}

}  // namespace sync_file_system
