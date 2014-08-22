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
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/sync_file_system/conflict_resolution_policy.h"
#include "chrome/browser/sync_file_system/file_status_observer.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_process_runner.h"
#include "chrome/browser/sync_file_system/sync_service_state.h"
#include "chrome/browser/sync_file_system/task_logger.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry_observer.h"
#include "url/gurl.h"

class Profile;
class ProfileSyncServiceBase;

namespace storage {
class FileSystemContext;
}

namespace sync_file_system {

class LocalFileSyncService;
class LocalSyncRunner;
class RemoteSyncRunner;
class SyncEventObserver;

class SyncFileSystemService
    : public KeyedService,
      public SyncProcessRunner::Client,
      public ProfileSyncServiceObserver,
      public FileStatusObserver,
      public extensions::ExtensionRegistryObserver,
      public base::SupportsWeakPtr<SyncFileSystemService> {
 public:
  typedef base::Callback<void(const base::ListValue&)> DumpFilesCallback;
  typedef base::Callback<void(const RemoteFileSyncService::OriginStatusMap&)>
      ExtensionStatusMapCallback;

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  void InitializeForApp(storage::FileSystemContext* file_system_context,
                        const GURL& app_origin,
                        const SyncStatusCallback& callback);

  void GetExtensionStatusMap(const ExtensionStatusMapCallback& callback);
  void DumpFiles(const GURL& origin, const DumpFilesCallback& callback);
  void DumpDatabase(const DumpFilesCallback& callback);

  // Returns the file |url|'s sync status.
  void GetFileSyncStatus(const storage::FileSystemURL& url,
                         const SyncFileStatusCallback& callback);

  void AddSyncEventObserver(SyncEventObserver* observer);
  void RemoveSyncEventObserver(SyncEventObserver* observer);

  LocalChangeProcessor* GetLocalChangeProcessor(const GURL& origin);

  // SyncProcessRunner::Client implementations.
  virtual void OnSyncIdle() OVERRIDE;
  virtual SyncServiceState GetSyncServiceState() OVERRIDE;
  virtual SyncFileSystemService* GetSyncService() OVERRIDE;

  void OnPromotionCompleted(int* num_running_jobs);
  void CheckIfIdle();

  TaskLogger* task_logger() { return &task_logger_; }

  void CallOnIdleForTesting(const base::Closure& callback);

 private:
  friend class SyncFileSystemServiceFactory;
  friend class SyncFileSystemServiceTest;
  friend struct base::DefaultDeleter<SyncFileSystemService>;
  friend class LocalSyncRunner;
  friend class RemoteSyncRunner;

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
  void DidDumpFiles(const GURL& app_origin,
                    const DumpFilesCallback& callback,
                    scoped_ptr<base::ListValue> files);

  void DidDumpDatabase(const DumpFilesCallback& callback,
                       scoped_ptr<base::ListValue> list);

  void DidGetExtensionStatusMap(
      const ExtensionStatusMapCallback& callback,
      scoped_ptr<RemoteFileSyncService::OriginStatusMap> status_map);

  // Overrides sync_enabled_ setting. This should be called only by tests.
  void SetSyncEnabledForTesting(bool enabled);

  void DidGetLocalChangeStatus(const SyncFileStatusCallback& callback,
                               SyncStatusCode status,
                               bool has_pending_local_changes);

  void OnRemoteServiceStateUpdated(RemoteServiceState state,
                                   const std::string& description);

  // extensions::ExtensionRegistryObserver implementations.
  virtual void OnExtensionInstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      bool is_update) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;
  virtual void OnExtensionLoaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension) OVERRIDE;

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

  // SyncFileStatusObserver implementation.
  virtual void OnFileStatusChanged(const storage::FileSystemURL& url,
                                   SyncFileStatus sync_status,
                                   SyncAction action_taken,
                                   SyncDirection direction) OVERRIDE;

  // Check the profile's sync preference settings and call
  // remote_file_service_->SetSyncEnabled() to update the status.
  // |profile_sync_service| must be non-null.
  void UpdateSyncEnabledStatus(ProfileSyncServiceBase* profile_sync_service);

  // Runs the SyncProcessRunner method of all sync runners (e.g. for Local sync
  // and Remote sync).
  void RunForEachSyncRunners(void(SyncProcessRunner::*method)());

  // Returns the appropriate RemoteFileSyncService for the given origin/app.
  // (crbug.com/324215)
  RemoteFileSyncService* GetRemoteService(const GURL& origin);

  Profile* profile_;

  scoped_ptr<LocalFileSyncService> local_service_;
  scoped_ptr<RemoteFileSyncService> remote_service_;

  // Holds all SyncProcessRunners.
  ScopedVector<SyncProcessRunner> local_sync_runners_;
  ScopedVector<SyncProcessRunner> remote_sync_runners_;

  // Indicates if sync is currently enabled or not.
  bool sync_enabled_;

  TaskLogger task_logger_;
  ObserverList<SyncEventObserver> observers_;

  bool promoting_demoted_changes_;
  base::Closure idle_callback_;

  DISALLOW_COPY_AND_ASSIGN(SyncFileSystemService);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_SERVICE_H_
