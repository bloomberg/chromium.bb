// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/remote_to_local_syncer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_util.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

bool BuildFileSystemURL(
    MetadataDatabase* metadata_database,
    const FileTracker& tracker,
    fileapi::FileSystemURL* url) {
  base::FilePath path;
  if (!metadata_database->BuildPathForTracker(
          tracker.tracker_id(), &path))
    return false;

  GURL origin =
      extensions::Extension::GetBaseURLFromExtensionId(tracker.app_id());
  *url = sync_file_system::CreateSyncableFileSystemURL(origin, path);

  return true;
}

bool HasFolderAsParent(const FileDetails& details,
                       const std::string& folder_id) {
  for (int i = 0; i < details.parent_folder_ids_size(); ++i) {
    if (details.parent_folder_ids(i) == folder_id)
      return true;
  }
  return false;
}

bool HasDisabledAppRoot(MetadataDatabase* database,
                        const FileTracker& tracker) {
  DCHECK(tracker.active());
  FileTracker app_root_tracker;
  if (database->FindAppRootTracker(tracker.app_id(), &app_root_tracker)) {
    DCHECK(app_root_tracker.tracker_kind() == TRACKER_KIND_APP_ROOT ||
           app_root_tracker.tracker_kind() == TRACKER_KIND_DISABLED_APP_ROOT);
    return app_root_tracker.tracker_kind() == TRACKER_KIND_DISABLED_APP_ROOT;
  }
  return false;
}

scoped_ptr<FileMetadata> GetFileMetadata(MetadataDatabase* database,
                                         const std::string& file_id) {
  scoped_ptr<FileMetadata> metadata(new FileMetadata);
  if (!database->FindFileByFileID(file_id, metadata.get()))
    metadata.reset();
  return metadata.Pass();
}

}  // namespace

RemoteToLocalSyncer::RemoteToLocalSyncer(SyncEngineContext* sync_context,
                                         int priorities)
    : sync_context_(sync_context),
      priorities_(priorities),
      weak_ptr_factory_(this) {
}

RemoteToLocalSyncer::~RemoteToLocalSyncer() {
  NOTIMPLEMENTED();
}

void RemoteToLocalSyncer::Run(const SyncStatusCallback& callback) {
  SyncStatusCallback wrapped_callback = base::Bind(
      &RemoteToLocalSyncer::SyncCompleted, weak_ptr_factory_.GetWeakPtr(),
      callback);

  if (priorities_ & PRIORITY_NORMAL) {
    dirty_tracker_ = make_scoped_ptr(new FileTracker);
    if (metadata_database()->GetNormalPriorityDirtyTracker(
            dirty_tracker_.get())) {
      ResolveRemoteChange(wrapped_callback);
      return;
    }
  }

  if (priorities_ & PRIORITY_LOW) {
    dirty_tracker_ = make_scoped_ptr(new FileTracker);
    if (metadata_database()->GetLowPriorityDirtyTracker(dirty_tracker_.get())) {
      ResolveRemoteChange(wrapped_callback);
      return;
    }
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, SYNC_STATUS_NO_CHANGE_TO_SYNC));
}

void RemoteToLocalSyncer::ResolveRemoteChange(
    const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);
  remote_metadata_ = GetFileMetadata(
      metadata_database(), dirty_tracker_->file_id());

  if (!remote_metadata_ || !remote_metadata_->has_details()) {
    if (remote_metadata_ && !remote_metadata_->has_details()) {
      LOG(ERROR) << "Missing details of a remote file: "
                 << remote_metadata_->file_id();
      NOTREACHED();
    }
    HandleMissingRemoteMetadata(callback);
    return;
  }

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  const FileDetails& remote_details = remote_metadata_->details();

  if (!dirty_tracker_->active() ||
      HasDisabledAppRoot(metadata_database(), *dirty_tracker_)) {
    HandleInactiveTracker(callback);
    return;
  }
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));

  if (!dirty_tracker_->has_synced_details() ||
      dirty_tracker_->synced_details().deleted()) {
    if (remote_details.deleted()) {
      // Remote deletion to local missing file.
      if (dirty_tracker_->has_synced_details() &&
          dirty_tracker_->synced_details().deleted()) {
        // This should be handled by MetadataDatabase.
        // MetadataDatabase should drop a tracker that marked as deleted if
        // corresponding file metadata is marked as deleted.
        LOG(ERROR) << "Found a stray deleted tracker: "
                   << dirty_tracker_->file_id();
        NOTREACHED();
      }

      callback.Run(SYNC_STATUS_OK);
      return;
    }
    DCHECK(!remote_details.deleted());

    if (remote_details.file_kind() == FILE_KIND_UNSUPPORTED) {
      // All unsupported file must be inactive.
      LOG(ERROR) << "Found an unsupported active file: "
                 << remote_metadata_->file_id();
      NOTREACHED();
      callback.Run(SYNC_STATUS_FAILED);
      return;
    }
    DCHECK(remote_details.file_kind() == FILE_KIND_FILE ||
           remote_details.file_kind() == FILE_KIND_FOLDER);

    if (remote_details.file_kind() == FILE_KIND_FILE) {
      HandleNewFile(callback);
      return;
    }

    DCHECK(remote_details.file_kind() == FILE_KIND_FOLDER);
    HandleNewFolder(callback);
    return;
  }
  DCHECK(dirty_tracker_->has_synced_details());
  DCHECK(!dirty_tracker_->synced_details().deleted());
  const FileDetails& synced_details = dirty_tracker_->synced_details();

  if (remote_details.deleted()) {
    HandleDeletion(callback);
    return;
  }

  // Most of remote_details field is valid from here.
  DCHECK(!remote_details.deleted());

  if (synced_details.file_kind() != remote_details.file_kind()) {
    LOG(ERROR) << "Found type mismatch between remote and local file: "
               << dirty_tracker_->file_id()
               << " type: (local) " << synced_details.file_kind()
               << " vs (remote) " << remote_details.file_kind();
    NOTREACHED();
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }
  DCHECK_EQ(synced_details.file_kind(), remote_details.file_kind());

  if (synced_details.file_kind() == FILE_KIND_UNSUPPORTED) {
    LOG(ERROR) << "Found an unsupported active file: "
               << remote_metadata_->file_id();
    NOTREACHED();
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }
  DCHECK(remote_details.file_kind() == FILE_KIND_FILE ||
         remote_details.file_kind() == FILE_KIND_FOLDER);

  if (synced_details.title() != remote_details.title()) {
    HandleRename(callback);
    return;
  }
  DCHECK_EQ(synced_details.title(), remote_details.title());

  if (dirty_tracker_->tracker_id() !=
      metadata_database()->GetSyncRootTrackerID()) {
    FileTracker parent_tracker;
    if (!metadata_database()->FindTrackerByTrackerID(
            dirty_tracker_->parent_tracker_id(), &parent_tracker)) {
      LOG(ERROR) << "Missing parent tracker for a non sync-root tracker: "
                 << dirty_tracker_->file_id();
      NOTREACHED();
      callback.Run(SYNC_STATUS_FAILED);
      return;
    }

    if (!HasFolderAsParent(remote_details, parent_tracker.file_id())) {
      HandleReorganize(callback);
      return;
    }
  }

  if (synced_details.file_kind() == FILE_KIND_FILE) {
    if (synced_details.md5() != remote_details.md5()) {
      HandleContentUpdate(callback);
      return;
    }
  } else {
    DCHECK_EQ(FILE_KIND_FOLDER, synced_details.file_kind());
    if (dirty_tracker_->needs_folder_listing()) {
      HandleFolderContentListing(callback);
      return;
    }
  }

  HandleOfflineSolvable(callback);
}

void RemoteToLocalSyncer::HandleMissingRemoteMetadata(
    const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);

  drive_service()->GetResourceEntry(
      dirty_tracker_->file_id(),
      base::Bind(&RemoteToLocalSyncer::DidGetRemoteMetadata,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 metadata_database()->GetLargestKnownChangeID()));
}

void RemoteToLocalSyncer::DidGetRemoteMetadata(
    const SyncStatusCallback& callback,
    int64 change_id,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  metadata_database()->UpdateByFileResource(
      change_id,
      *drive::util::ConvertResourceEntryToFileResource(*entry),
      base::Bind(&RemoteToLocalSyncer::DidUpdateDatabaseForRemoteMetadata,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void RemoteToLocalSyncer::DidUpdateDatabaseForRemoteMetadata(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  callback.Run(SYNC_STATUS_RETRY);  // Do not update |dirty_tracker_|.
}

void RemoteToLocalSyncer::HandleInactiveTracker(
    const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);
  DCHECK(!dirty_tracker_->active() ||
         HasDisabledAppRoot(metadata_database(), *dirty_tracker_));

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());

  NOTIMPLEMENTED();
  callback.Run(SYNC_STATUS_FAILED);
}

void RemoteToLocalSyncer::HandleNewFile(
    const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));
  DCHECK(!dirty_tracker_->has_synced_details() ||
         dirty_tracker_->synced_details().deleted());

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(!remote_metadata_->details().deleted());
  DCHECK_EQ(FILE_KIND_FILE, remote_metadata_->details().file_kind());

  Prepare(base::Bind(&RemoteToLocalSyncer::DidPrepareForNewFile,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void RemoteToLocalSyncer::DidPrepareForNewFile(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  DCHECK(url_.is_valid());
  DCHECK(local_metadata_);
  DCHECK(local_changes_);

  // Check if the local file exists.
  if (local_metadata_->file_type == SYNC_FILE_TYPE_UNKNOWN ||
      (!local_changes_->empty() && local_changes_->back().IsDelete())) {
    // Missing local file case.
    // Download the file and add it to local as a new file.
    DownloadFile(callback);
    return;
  }

  DCHECK(local_changes_->empty() || local_changes_->back().IsAddOrUpdate());
  if (local_changes_->empty()) {
    // No local change for the local file.
    LOG(ERROR) << "Detected local-only file without pending local change: "
               << url_.DebugString();

    if (local_metadata_->file_type == SYNC_FILE_TYPE_FILE) {
      // Download the file and overwrite the existing local file.
      DownloadFile(callback);
      return;
    }

    DCHECK_EQ(SYNC_FILE_TYPE_DIRECTORY, local_metadata_->file_type);
    // Got a remote regular file modification for existing local folder.
    // Our policy prioritize folders in this case.
    // TODO(tzik): Inactivate the file and activate one of the folder at the
    // path.
    NOTIMPLEMENTED();
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  DCHECK(local_changes_->back().IsAddOrUpdate());
  // Conflict case.
  // Do nothing for the change now, and handle this in LocalToRemoteSync phase.

  // Lower the priority of the tracker to prevent repeated remote sync to the
  // same tracker.
  metadata_database()->LowerTrackerPriority(dirty_tracker_->tracker_id());
  callback.Run(SYNC_STATUS_RETRY);
}

void RemoteToLocalSyncer::HandleNewFolder(const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(!remote_metadata_->details().deleted());
  DCHECK_EQ(FILE_KIND_FOLDER, remote_metadata_->details().file_kind());

  Prepare(base::Bind(&RemoteToLocalSyncer::DidPrepareForNewFolder,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void RemoteToLocalSyncer::DidPrepareForNewFolder(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  DCHECK(url_.is_valid());
  DCHECK(local_metadata_);
  DCHECK(local_changes_);

  // Check if the local file exists.
  if (local_metadata_->file_type == SYNC_FILE_TYPE_UNKNOWN ||
      (!local_changes_->empty() && local_changes_->back().IsDelete())) {
    // No local file exists at the path.
    CreateFolder(callback);
    return;
  }

  DCHECK(local_changes_->empty() || local_changes_->back().IsAddOrUpdate());
  if (local_changes_->empty()) {
    LOG(ERROR) << "Detected local-only file without pending local change: "
               << url_.DebugString();
  }

  if (local_metadata_->file_type == SYNC_FILE_TYPE_DIRECTORY) {
    // There already exists a folder, nothing left to do.
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  DCHECK_EQ(SYNC_FILE_TYPE_FILE, local_metadata_->file_type);
  // Got a remote folder for existing local file.
  // Our policy prioritize folders in this case.
  CreateFolder(callback);
}

void RemoteToLocalSyncer::HandleDeletion(
    const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));
  DCHECK(dirty_tracker_->has_synced_details());
  DCHECK(!dirty_tracker_->synced_details().deleted());

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(remote_metadata_->details().deleted());

  Prepare(base::Bind(&RemoteToLocalSyncer::DidPrepareForDeletion,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void RemoteToLocalSyncer::DidPrepareForDeletion(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  DCHECK(url_.is_valid());
  DCHECK(local_metadata_);
  DCHECK(local_changes_);

  // Check if the local file exists.
  if (local_metadata_->file_type == SYNC_FILE_TYPE_UNKNOWN ||
      (!local_changes_->empty() && local_changes_->back().IsDelete())) {
    // No local file exists at the path.
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  DCHECK(local_changes_->empty() || local_changes_->back().IsAddOrUpdate());
  if (local_changes_->empty()) {
    DeleteLocalFile(callback);
    return;
  }

  DCHECK(local_changes_->back().IsAddOrUpdate());
  // File is remotely deleted and locally updated.
  // Ignore the remote deletion and handle it as if applied successfully.
  callback.Run(SYNC_STATUS_OK);
}

void RemoteToLocalSyncer::HandleRename(
    const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));
  DCHECK(dirty_tracker_->has_synced_details());
  DCHECK(!dirty_tracker_->synced_details().deleted());

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(!remote_metadata_->details().deleted());

  DCHECK_EQ(dirty_tracker_->synced_details().file_kind(),
            remote_metadata_->details().file_kind());
  DCHECK_NE(dirty_tracker_->synced_details().title(),
            remote_metadata_->details().title());

  NOTIMPLEMENTED();
  callback.Run(SYNC_STATUS_FAILED);
}

void RemoteToLocalSyncer::HandleReorganize(
    const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));
  DCHECK(dirty_tracker_->has_synced_details());
  DCHECK(!dirty_tracker_->synced_details().deleted());

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(!remote_metadata_->details().deleted());

  NOTIMPLEMENTED();
  callback.Run(SYNC_STATUS_FAILED);
}

void RemoteToLocalSyncer::HandleContentUpdate(
    const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));
  DCHECK(dirty_tracker_->has_synced_details());
  DCHECK(!dirty_tracker_->synced_details().deleted());
  DCHECK_EQ(FILE_KIND_FILE, dirty_tracker_->synced_details().file_kind());

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(!remote_metadata_->details().deleted());

  DCHECK_NE(dirty_tracker_->synced_details().md5(),
            remote_metadata_->details().md5());

  Prepare(base::Bind(&RemoteToLocalSyncer::DidPrepareForContentUpdate,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void RemoteToLocalSyncer::DidPrepareForContentUpdate(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  NOTIMPLEMENTED();
  callback.Run(SYNC_STATUS_FAILED);
}

void RemoteToLocalSyncer::HandleFolderContentListing(
    const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));
  DCHECK(dirty_tracker_->has_synced_details());
  DCHECK(!dirty_tracker_->synced_details().deleted());
  DCHECK_EQ(FILE_KIND_FOLDER, dirty_tracker_->synced_details().file_kind());
  DCHECK(dirty_tracker_->needs_folder_listing());

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(!remote_metadata_->details().deleted());

  Prepare(base::Bind(&RemoteToLocalSyncer::DidPrepareForFolderListing,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void RemoteToLocalSyncer::DidPrepareForFolderListing(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  NOTIMPLEMENTED();
  callback.Run(SYNC_STATUS_FAILED);
}

void RemoteToLocalSyncer::HandleOfflineSolvable(
    const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));
  DCHECK(dirty_tracker_->has_synced_details());
  DCHECK(!dirty_tracker_->synced_details().deleted());

  DCHECK((dirty_tracker_->synced_details().file_kind() == FILE_KIND_FOLDER &&
          !dirty_tracker_->needs_folder_listing()) ||
         (dirty_tracker_->synced_details().file_kind() == FILE_KIND_FILE &&
          dirty_tracker_->synced_details().md5() ==
          remote_metadata_->details().md5()));

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(!remote_metadata_->details().deleted());

  NOTIMPLEMENTED();
  callback.Run(SYNC_STATUS_FAILED);
}

void RemoteToLocalSyncer::SyncCompleted(const SyncStatusCallback& callback,
                                        SyncStatusCode status) {
  if (status == SYNC_STATUS_RETRY) {
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  DCHECK(dirty_tracker_);
  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  metadata_database()->UpdateTracker(dirty_tracker_->tracker_id(),
                                     remote_metadata_->details(),
                                     callback);
}

void RemoteToLocalSyncer::Prepare(const SyncStatusCallback& callback) {
  bool should_success = BuildFileSystemURL(
      metadata_database(), *dirty_tracker_, &url_);
  DCHECK(should_success);
  DCHECK(url_.is_valid());
  remote_change_processor()->PrepareForProcessRemoteChange(
      url_,
      base::Bind(&RemoteToLocalSyncer::DidPrepare,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void RemoteToLocalSyncer::DidPrepare(const SyncStatusCallback& callback,
                                     SyncStatusCode status,
                                     const SyncFileMetadata& local_metadata,
                                     const FileChangeList& local_changes) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  local_metadata_.reset(new SyncFileMetadata(local_metadata));
  local_changes_.reset(new FileChangeList(local_changes));

  callback.Run(status);
}

void RemoteToLocalSyncer::DeleteLocalFile(const SyncStatusCallback& callback) {
  if (dirty_tracker_->tracker_id() ==
      metadata_database()->GetSyncRootTrackerID()) {
    // TODO(tzik): Sync-root is deleted. Needs special handling.
    NOTIMPLEMENTED();
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  if (dirty_tracker_->tracker_kind() == TRACKER_KIND_APP_ROOT) {
    // TODO(tzik): Active app-root is deleted. Needs special handling.
    NOTIMPLEMENTED();
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  remote_change_processor()->ApplyRemoteChange(
      FileChange(FileChange::FILE_CHANGE_DELETE, SYNC_FILE_TYPE_UNKNOWN),
      base::FilePath(),
      url_,
      callback);
}

void RemoteToLocalSyncer::DownloadFile(const SyncStatusCallback& callback) {
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&sync_file_system::drive_backend::CreateTemporaryFile),
      base::Bind(&RemoteToLocalSyncer::DidCreateTemporaryFileForDownload,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void RemoteToLocalSyncer::DidCreateTemporaryFileForDownload(
    const SyncStatusCallback& callback,
    webkit_blob::ScopedFile file) {
  base::FilePath path = file.path();
  drive_service()->DownloadFile(
      path, remote_metadata_->file_id(),
      base::Bind(&RemoteToLocalSyncer::DidDownloadFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback, base::Passed(&file)),
      google_apis::GetContentCallback(),
      google_apis::ProgressCallback());
}

void RemoteToLocalSyncer::DidDownloadFile(const SyncStatusCallback& callback,
                                          webkit_blob::ScopedFile file,
                                          google_apis::GDataErrorCode error,
                                          const base::FilePath&) {
  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(GDataErrorCodeToSyncStatusCode(error));
    return;
  }

  base::FilePath path = file.path();
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&drive::util::GetMd5Digest, path),
      base::Bind(&RemoteToLocalSyncer::DidCalculateMD5ForDownload,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback, base::Passed(&file)));
}

void RemoteToLocalSyncer::DidCalculateMD5ForDownload(
    const SyncStatusCallback& callback,
    webkit_blob::ScopedFile file,
    const std::string& md5) {
  if (md5.empty()) {
    callback.Run(SYNC_FILE_ERROR_NOT_FOUND);
    return;
  }

  if (md5 != remote_metadata_->details().md5()) {
    // File has been modified since last metadata retrieval.

    // Lower the priority of the tracker to prevent repeated remote sync to the
    // same tracker.
    metadata_database()->LowerTrackerPriority(dirty_tracker_->tracker_id());
    callback.Run(SYNC_STATUS_RETRY);
    return;
  }

  base::FilePath path = file.path();
  remote_change_processor()->ApplyRemoteChange(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE, SYNC_FILE_TYPE_FILE),
      path, url_,
      base::Bind(&RemoteToLocalSyncer::DidApplyDownload,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback, base::Passed(&file)));
}

void RemoteToLocalSyncer::DidApplyDownload(const SyncStatusCallback& callback,
                                           webkit_blob::ScopedFile,
                                           SyncStatusCode status) {
  callback.Run(status);
}

void RemoteToLocalSyncer::CreateFolder(const SyncStatusCallback& callback) {
  remote_change_processor()->ApplyRemoteChange(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY),
      base::FilePath(), url_,
      callback);
}

drive::DriveServiceInterface* RemoteToLocalSyncer::drive_service() {
  return sync_context_->GetDriveService();
}

MetadataDatabase* RemoteToLocalSyncer::metadata_database() {
  return sync_context_->GetMetadataDatabase();
}

RemoteChangeProcessor* RemoteToLocalSyncer::remote_change_processor() {
  DCHECK(sync_context_->GetRemoteChangeProcessor());
  return sync_context_->GetRemoteChangeProcessor();
}

}  // namespace drive_backend
}  // namespace sync_file_system
