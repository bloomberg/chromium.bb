// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_SERVICE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_SERVICE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/sync_file_system/conflict_resolution_policy.h"
#include "chrome/browser/sync_file_system/file_status_observer.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_service_state.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "url/gurl.h"

class ProfileSyncServiceBase;

namespace fileapi {
class FileSystemContext;
}

namespace sync_file_system {

class SyncEventObserver;

class SyncFileSystemService
    : public BrowserContextKeyedService,
      public ProfileSyncServiceObserver,
      public LocalFileSyncService::Observer,
      public RemoteFileSyncService::Observer,
      public FileStatusObserver,
      public content::NotificationObserver,
      public base::SupportsWeakPtr<SyncFileSystemService> {
 public:
  typedef base::Callback<void(const base::ListValue* files)> DumpFilesCallback;

  // BrowserContextKeyedService overrides.
  virtual void Shutdown() OVERRIDE;

  void InitializeForApp(
      fileapi::FileSystemContext* file_system_context,
      const GURL& app_origin,
      const SyncStatusCallback& callback);

  SyncServiceState GetSyncServiceState();
  void GetExtensionStatusMap(std::map<GURL, std::string>* status_map);
  void DumpFiles(const GURL& origin, const DumpFilesCallback& callback);

  // Returns the file |url|'s sync status.
  void GetFileSyncStatus(
      const fileapi::FileSystemURL& url,
      const SyncFileStatusCallback& callback);

  void AddSyncEventObserver(SyncEventObserver* observer);
  void RemoveSyncEventObserver(SyncEventObserver* observer);

  ConflictResolutionPolicy GetConflictResolutionPolicy() const;
  SyncStatusCode SetConflictResolutionPolicy(ConflictResolutionPolicy policy);

 private:
  class SyncRunner;

  friend class SyncFileSystemServiceFactory;
  friend class SyncFileSystemServiceTest;
  friend struct base::DefaultDeleter<SyncFileSystemService>;

  explicit SyncFileSystemService(Profile* profile);
  virtual ~SyncFileSystemService();

  void Initialize(scoped_ptr<LocalFileSyncService> local_file_service,
                  scoped_ptr<RemoteFileSyncService> remote_file_service);

  // Callbacks for InitializeForApp.
  void DidInitializeFileSystem(const GURL& app_origin,
                               const SyncStatusCallback& callback,
                               SyncStatusCode status);
  void DidRegisterOrigin(const GURL& app_origin,
                         const SyncStatusCallback& callback,
                         SyncStatusCode status);

  void DidInitializeFileSystemForDump(const GURL& app_origin,
                                      const DumpFilesCallback& callback,
                                      SyncStatusCode status);

  // Overrides sync_enabled_ setting. This should be called only by tests.
  void SetSyncEnabledForTesting(bool enabled);

  void StartLocalSync(const SyncStatusCallback& callback);
  void StartRemoteSync(const SyncStatusCallback& callback);

  // Callbacks for remote/local sync.
  void DidProcessRemoteChange(const SyncStatusCallback& callback,
                              SyncStatusCode status,
                              const fileapi::FileSystemURL& url);
  void DidProcessLocalChange(const SyncStatusCallback& callback,
                             SyncStatusCode status,
                             const fileapi::FileSystemURL& url);

  void DidGetLocalChangeStatus(const SyncFileStatusCallback& callback,
                               SyncStatusCode status,
                               bool has_pending_local_changes);

  void OnSyncEnabledForRemoteSync();

  // RemoteFileSyncService::Observer overrides.
  virtual void OnLocalChangeAvailable(int64 pending_changes) OVERRIDE;

  // LocalFileSyncService::Observer overrides.
  virtual void OnRemoteChangeQueueUpdated(int64 pending_changes) OVERRIDE;
  virtual void OnRemoteServiceStateUpdated(
      RemoteServiceState state,
      const std::string& description) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void HandleExtensionInstalled(const content::NotificationDetails& details);
  void HandleExtensionUnloaded(int type,
                               const content::NotificationDetails& details);
  void HandleExtensionUninstalled(int type,
                                  const content::NotificationDetails& details);
  void HandleExtensionEnabled(int type,
                              const content::NotificationDetails& details);

  // ProfileSyncServiceObserver:
  virtual void OnStateChanged() OVERRIDE;

  // SyncFileStatusObserver:
  virtual void OnFileStatusChanged(
      const fileapi::FileSystemURL& url,
      SyncFileStatus sync_status,
      SyncAction action_taken,
      SyncDirection direction) OVERRIDE;

  // Check the profile's sync preference settings and call
  // remote_file_service_->SetSyncEnabled() to update the status.
  // |profile_sync_service| must be non-null.
  void UpdateSyncEnabledStatus(ProfileSyncServiceBase* profile_sync_service);

  Profile* profile_;
  content::NotificationRegistrar registrar_;

  scoped_ptr<LocalFileSyncService> local_file_service_;
  scoped_ptr<RemoteFileSyncService> remote_file_service_;

  scoped_ptr<SyncRunner> local_sync_;
  scoped_ptr<SyncRunner> remote_sync_;

  // Indicates if sync is currently enabled or not.
  bool sync_enabled_;

  ObserverList<SyncEventObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(SyncFileSystemService);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_SERVICE_H_
