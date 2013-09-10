// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_LOCAL_SYNC_DELEGATE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_LOCAL_SYNC_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace sync_file_system {

class ConflictResolutionResolver;
class DriveMetadataStore;

namespace drive_backend {

class APIUtil;

// This class handles ApplyLocalChange in LocalChangeProcessor, and its instance
// represents single ApplyLocalChange operation.
// The caller is responsible to own the instance, and can cancel operation by
// deleting the instance or |sync_service|.
class LocalSyncDelegate {
 public:
  typedef RemoteChangeHandler::RemoteChange RemoteChange;

  LocalSyncDelegate(DriveFileSyncService* sync_service,
                    const FileChange& change,
                    const base::FilePath& local_file_path,
                    const SyncFileMetadata& local_file_metadata,
                    const fileapi::FileSystemURL& url);
  ~LocalSyncDelegate();

  void Run(const SyncStatusCallback& callback);

 private:
  void DidGetOriginRoot(const SyncStatusCallback& callback,
                        SyncStatusCode status,
                        const std::string& resource_id);
  void UploadNewFile(const SyncStatusCallback& callback);
  void DidUploadNewFile(const SyncStatusCallback& callback,
                        google_apis::GDataErrorCode error,
                        const std::string& resource_id,
                        const std::string& md5);
  void CreateDirectory(const SyncStatusCallback& callback);
  void DidCreateDirectory(
      const SyncStatusCallback& callback,
      google_apis::GDataErrorCode error,
      const std::string& resource_id);
  void UploadExistingFile(const SyncStatusCallback& callback);
  void DidUploadExistingFile(
      const SyncStatusCallback& callback,
      google_apis::GDataErrorCode error,
      const std::string& resource_id,
      const std::string& md5);
  void Delete(const SyncStatusCallback& callback);
  void DidDelete(const SyncStatusCallback& callback,
                 google_apis::GDataErrorCode error);
  void DidDeleteMetadataForDeletionConflict(
      const SyncStatusCallback& callback,
      SyncStatusCode status);
  void ResolveToLocal(const SyncStatusCallback& callback);
  void DidDeleteFileToResolveToLocal(
      const SyncStatusCallback& callback,
      google_apis::GDataErrorCode error);
  void ResolveToRemote(const SyncStatusCallback& callback,
                       SyncFileType remote_file_type);
  void DidResolveToRemote(const SyncStatusCallback& callback,
                          SyncStatusCode status);
  void DidApplyLocalChange(
      const SyncStatusCallback& callback,
      const google_apis::GDataErrorCode error,
      SyncStatusCode status);

  // Metadata manipulation.
  void UpdateMetadata(const std::string& resource_id,
                      const std::string& md5,
                      DriveMetadata::ResourceType type,
                      const SyncStatusCallback& callback);
  void ResetMetadataForStartOver(const SyncStatusCallback& callback);
  void SetMetadataToBeFetched(DriveMetadata::ResourceType type,
                              const SyncStatusCallback& callback);
  void DeleteMetadata(const SyncStatusCallback& callback);
  void SetMetadataConflict(const SyncStatusCallback& callback);

  // Conflict handling.
  void HandleCreationConflict(
      const std::string& resource_id,
      DriveMetadata::ResourceType type,
      const SyncStatusCallback& callback);
  void HandleConflict(const SyncStatusCallback& callback);
  void DidGetEntryForConflictResolution(
      const SyncStatusCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceEntry> entry);

  void HandleManualResolutionCase(const SyncStatusCallback& callback);
  void DidMarkConflict(const SyncStatusCallback& callback,
                       SyncStatusCode status);

  void HandleLocalWinCase(const SyncStatusCallback& callback);
  void HandleRemoteWinCase(const SyncStatusCallback& callback,
                           SyncFileType remote_file_type);
  void StartOver(const SyncStatusCallback& callback, SyncStatusCode status);

  SyncStatusCode GDataErrorCodeToSyncStatusCodeWrapper(
      google_apis::GDataErrorCode error);

  DriveMetadataStore* metadata_store();
  APIUtilInterface* api_util();
  RemoteChangeHandler* remote_change_handler();
  ConflictResolutionResolver* conflict_resolution_resolver();

  DriveFileSyncService* sync_service_;  // Not owned.

  SyncOperationType operation_;

  fileapi::FileSystemURL url_;
  FileChange local_change_;
  base::FilePath local_path_;
  SyncFileMetadata local_metadata_;
  DriveMetadata drive_metadata_;
  bool has_drive_metadata_;
  RemoteChange remote_change_;
  bool has_remote_change_;

  std::string origin_resource_id_;

  base::WeakPtrFactory<LocalSyncDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalSyncDelegate);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_LOCAL_SYNC_DELEGATE_H_
