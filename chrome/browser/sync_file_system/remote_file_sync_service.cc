// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/remote_file_sync_service.h"

#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_service.h"

namespace sync_file_system {

scoped_ptr<RemoteFileSyncService>
RemoteFileSyncService::CreateForBrowserContext(
    BackendVersion version,
    content::BrowserContext* context,
    TaskLogger* task_logger) {
  switch (version) {
    case V1:
      return DriveFileSyncService::Create(
          Profile::FromBrowserContext(context)).PassAs<RemoteFileSyncService>();
    case V2:
      return drive_backend::SyncEngine::CreateForBrowserContext(
          context, task_logger).PassAs<RemoteFileSyncService>();
  }
  NOTREACHED() << "Unknown version " << version;
  return scoped_ptr<RemoteFileSyncService>();
}

void RemoteFileSyncService::AppendDependsOnFactories(
    BackendVersion version,
    std::set<BrowserContextKeyedServiceFactory*>* factories) {
  switch (version) {
    case V1:
      DriveFileSyncService::AppendDependsOnFactories(factories);
      return;
    case V2:
      drive_backend::SyncEngine::AppendDependsOnFactories(factories);
      return;
  }
  NOTREACHED() << "Unknown version " << version;
}

}  // namespace sync_file_system
