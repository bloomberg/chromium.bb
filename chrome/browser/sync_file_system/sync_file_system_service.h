// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_SERVICE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/sync_file_system/change_observer_interface.h"
#include "webkit/fileapi/syncable/sync_callbacks.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

class GURL;

namespace fileapi {
class FileSystemContext;
}

namespace sync_file_system {

class LocalFileSyncService;
class LocalChangeObserver;
class RemoteChangeObserver;
class RemoteFileSyncService;

class SyncFileSystemService
    : public ProfileKeyedService,
      public LocalChangeObserver,
      public RemoteChangeObserver {
 public:
  // ProfileKeyedService overrides.
  virtual void Shutdown() OVERRIDE;

  void InitializeForApp(
      fileapi::FileSystemContext* file_system_context,
      const std::string& service_name,
      const GURL& app_url,
      const fileapi::StatusCallback& callback);

 private:
  friend class SyncFileSystemServiceFactory;

  explicit SyncFileSystemService(Profile* profile);
  virtual ~SyncFileSystemService();

  void Initialize(scoped_ptr<LocalFileSyncService> local_file_service,
                  scoped_ptr<RemoteFileSyncService> remote_file_service);

  // RemoteChangeObserver overrides.
  virtual void OnLocalChangeAvailable(int64 pending_changes) OVERRIDE;

  // LocalChangeObserver overrides.
  virtual void OnRemoteChangeAvailable(int64 pending_changes) OVERRIDE;

  Profile* profile_;

  int64 pending_local_changes_;
  int64 pending_remote_changes_;

  scoped_ptr<LocalFileSyncService> local_file_service_;
  scoped_ptr<RemoteFileSyncService> remote_file_service_;

  DISALLOW_COPY_AND_ASSIGN(SyncFileSystemService);
};

class SyncFileSystemServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static SyncFileSystemService* GetForProfile(Profile* profile);
  static SyncFileSystemService* FindForProfile(Profile* profile);
  static SyncFileSystemServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<SyncFileSystemServiceFactory>;
  SyncFileSystemServiceFactory();
  virtual ~SyncFileSystemServiceFactory();

  // ProfileKeyedServiceFactory overrides.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_SERVICE_H_
