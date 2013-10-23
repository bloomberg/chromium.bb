// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_REMOTE_SYNC_DELEGATE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_REMOTE_SYNC_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/api_util.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/sync_action.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/blob/scoped_file.h"

namespace sync_file_system {

class DriveMetadataStore;
class RemoteChangeProcessor;

namespace drive_backend {

class APIUtil;

// This class handles RemoteFileSyncService::ProcessRemoteChange for drive
// backend, and its instance represents single ProcessRemoteChange operation.
// The caller is responsible to own the instance, and can cancel operation by
// deleting the instance.
class RemoteSyncDelegate : public base::SupportsWeakPtr<RemoteSyncDelegate> {
 public:
  typedef RemoteChangeHandler::RemoteChange RemoteChange;

  RemoteSyncDelegate(
      DriveFileSyncService* sync_service,
      const RemoteChange& remote_change);
  virtual ~RemoteSyncDelegate();

  void Run(const SyncStatusCallback& callback);

  const fileapi::FileSystemURL& url() const { return remote_change_.url; }

 private:
  void DidPrepareForProcessRemoteChange(const SyncStatusCallback& callback,
                                        SyncStatusCode status,
                                        const SyncFileMetadata& metadata,
                                        const FileChangeList& local_changes);
  void ApplyRemoteChange(const SyncStatusCallback& callback);
  void DidApplyRemoteChange(const SyncStatusCallback& callback,
                            SyncStatusCode status);
  void DeleteMetadata(const SyncStatusCallback& callback);
  void DownloadFile(const SyncStatusCallback& callback);
  void DidDownloadFile(const SyncStatusCallback& callback,
                       google_apis::GDataErrorCode error,
                       const std::string& md5_checksum,
                       int64 file_size,
                       const base::Time& updated_time,
                       webkit_blob::ScopedFile downloaded_file);
  void HandleConflict(const SyncStatusCallback& callback,
                      SyncFileType remote_file_type);
  void HandleLocalWin(const SyncStatusCallback& callback);
  void HandleRemoteWin(const SyncStatusCallback& callback,
                       SyncFileType remote_file_type);
  void HandleManualResolutionCase(const SyncStatusCallback& callback);
  void DidGetEntryForConflictResolution(
      const SyncStatusCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceEntry> entry);
  void ResolveToLocal(const SyncStatusCallback& callback);
  void DidResolveToLocal(const SyncStatusCallback& callback,
                         SyncStatusCode status);
  void ResolveToRemote(const SyncStatusCallback& callback);
  void DidResolveToRemote(const SyncStatusCallback& callback,
                          SyncStatusCode status);
  void StartOver(const SyncStatusCallback& callback,
                 SyncStatusCode status);

  void CompleteSync(const SyncStatusCallback& callback,
                    SyncStatusCode status);
  void AbortSync(const SyncStatusCallback& callback,
                 SyncStatusCode status);
  void DidFinish(const SyncStatusCallback& callback,
                 SyncStatusCode status);
  void DispatchCallbackAfterDidFinish(const SyncStatusCallback& callback,
                                      SyncStatusCode status);

  SyncStatusCode GDataErrorCodeToSyncStatusCodeWrapper(
      google_apis::GDataErrorCode error);

  DriveMetadataStore* metadata_store();
  APIUtilInterface* api_util();
  RemoteChangeHandler* remote_change_handler();
  RemoteChangeProcessor* remote_change_processor();
  ConflictResolutionResolver* conflict_resolution_resolver();

  const FileChange& remote_file_change() const { return remote_change_.change; }

  DriveFileSyncService* sync_service_;  // Not owned.

  RemoteChange remote_change_;
  DriveMetadata drive_metadata_;
  SyncFileMetadata local_metadata_;
  webkit_blob::ScopedFile temporary_file_;
  std::string md5_checksum_;
  SyncAction sync_action_;
  bool metadata_updated_;
  bool clear_local_changes_;

  std::string origin_resource_id_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSyncDelegate);
};

}  // namespace drive_backend

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_REMOTE_SYNC_DELEGATE_H_
