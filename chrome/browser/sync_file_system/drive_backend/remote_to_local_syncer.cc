// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/remote_to_local_syncer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/sync_file_system/drive_backend/callback_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"
#include "chrome/browser/sync_file_system/drive_backend/task_dependency_manager.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "extensions/common/extension.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "webkit/common/fileapi/file_system_util.h"

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

// Creates a temporary file in |dir_path|.  This must be called on an
// IO-allowed task runner, and the runner must be given as |file_task_runner|.
webkit_blob::ScopedFile CreateTemporaryFile(
    base::TaskRunner* file_task_runner) {
  base::FilePath temp_file_path;
  if (!base::CreateTemporaryFile(&temp_file_path))
    return webkit_blob::ScopedFile();

  return webkit_blob::ScopedFile(
      temp_file_path,
      webkit_blob::ScopedFile::DELETE_ON_SCOPE_OUT,
      file_task_runner);
}

}  // namespace

RemoteToLocalSyncer::RemoteToLocalSyncer(SyncEngineContext* sync_context)
    : sync_context_(sync_context),
      sync_action_(SYNC_ACTION_NONE),
      prepared_(false),
      sync_root_deletion_(false),
      weak_ptr_factory_(this) {
}

RemoteToLocalSyncer::~RemoteToLocalSyncer() {
}

void RemoteToLocalSyncer::RunPreflight(scoped_ptr<SyncTaskToken> token) {
  token->InitializeTaskLog("Remote -> Local");

  scoped_ptr<BlockingFactor> blocking_factor(new BlockingFactor);
  blocking_factor->exclusive = true;
  SyncTaskManager::UpdateBlockingFactor(
      token.Pass(), blocking_factor.Pass(),
      base::Bind(&RemoteToLocalSyncer::RunExclusive,
                 weak_ptr_factory_.GetWeakPtr()));
}

void RemoteToLocalSyncer::RunExclusive(scoped_ptr<SyncTaskToken> token) {
  if (!drive_service() || !metadata_database() || !remote_change_processor()) {
    token->RecordLog("Context not ready.");
    NOTREACHED();
    SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_FAILED);
    return;
  }

  dirty_tracker_ = make_scoped_ptr(new FileTracker);
  if (metadata_database()->GetNormalPriorityDirtyTracker(
          dirty_tracker_.get())) {
    token->RecordLog(base::StringPrintf(
        "Start: tracker_id=%" PRId64, dirty_tracker_->tracker_id()));
    ResolveRemoteChange(token.Pass());
    return;
  }

  token->RecordLog("Nothing to do.");
  SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_NO_CHANGE_TO_SYNC);
}

void RemoteToLocalSyncer::ResolveRemoteChange(scoped_ptr<SyncTaskToken> token) {
  DCHECK(dirty_tracker_);
  remote_metadata_ = GetFileMetadata(
      metadata_database(), dirty_tracker_->file_id());

  if (!remote_metadata_ || !remote_metadata_->has_details()) {
    if (remote_metadata_ && !remote_metadata_->has_details()) {
      token->RecordLog(
          "Missing details of a remote file: " + remote_metadata_->file_id());
      NOTREACHED();
    }
    token->RecordLog("Missing remote metadata case.");
    HandleMissingRemoteMetadata(SyncCompletedCallback(token.Pass()));
    return;
  }

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  const FileDetails& remote_details = remote_metadata_->details();

  if (!dirty_tracker_->active() ||
      HasDisabledAppRoot(metadata_database(), *dirty_tracker_)) {
    // Handle inactive tracker in SyncCompleted.
    token->RecordLog("Inactive tracker case.");
    SyncCompleted(token.Pass(), SYNC_STATUS_OK);
    return;
  }

  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));

  if (!dirty_tracker_->has_synced_details()) {
    token->RecordLog(base::StringPrintf(
        "Missing synced_details of an active tracker: %" PRId64,
        dirty_tracker_->tracker_id()));
    NOTREACHED();
    SyncCompleted(token.Pass(), SYNC_STATUS_FAILED);
    return;
  }

  DCHECK(dirty_tracker_->has_synced_details());
  const FileDetails& synced_details = dirty_tracker_->synced_details();

  if (dirty_tracker_->tracker_id() ==
      metadata_database()->GetSyncRootTrackerID()) {
    if (remote_details.missing() ||
        synced_details.title() != remote_details.title() ||
        remote_details.parent_folder_ids_size()) {
      token->RecordLog("Sync-root deletion.");
      HandleSyncRootDeletion(SyncCompletedCallback(token.Pass()));
      return;
    }
    token->RecordLog("Trivial sync-root change.");
    SyncCompleted(token.Pass(), SYNC_STATUS_OK);
    return;
  }

  DCHECK_NE(dirty_tracker_->tracker_id(),
            metadata_database()->GetSyncRootTrackerID());

  if (remote_details.missing()) {
    if (!synced_details.missing()) {
      token->RecordLog("Remote file deletion.");
      HandleDeletion(SyncCompletedCallback(token.Pass()));
      return;
    }

    DCHECK(synced_details.missing());
    token->RecordLog("Found a stray missing tracker: " +
                     dirty_tracker_->file_id());
    NOTREACHED();
    SyncCompleted(token.Pass(), SYNC_STATUS_OK);
    return;
  }

  // Most of remote_details field is valid from here.
  DCHECK(!remote_details.missing());

  if (synced_details.file_kind() != remote_details.file_kind()) {
    token->RecordLog(base::StringPrintf(
        "Found type mismatch between remote and local file: %s"
        " type: (local) %d vs (remote) %d",
        dirty_tracker_->file_id().c_str(),
        synced_details.file_kind(),
        remote_details.file_kind()));
    NOTREACHED();
    SyncCompleted(token.Pass(), SYNC_STATUS_FAILED);
    return;
  }
  DCHECK_EQ(synced_details.file_kind(), remote_details.file_kind());

  if (synced_details.file_kind() == FILE_KIND_UNSUPPORTED) {
    token->RecordLog("Found an unsupported active file: " +
                     remote_metadata_->file_id());
    NOTREACHED();
    SyncCompleted(token.Pass(), SYNC_STATUS_FAILED);
    return;
  }
  DCHECK(remote_details.file_kind() == FILE_KIND_FILE ||
         remote_details.file_kind() == FILE_KIND_FOLDER);

  if (synced_details.title() != remote_details.title()) {
    // Handle rename as deletion + addition.
    token->RecordLog("Detected file rename.");
    Prepare(base::Bind(&RemoteToLocalSyncer::DidPrepareForDeletion,
                       weak_ptr_factory_.GetWeakPtr(),
                       SyncCompletedCallback(token.Pass())));
    return;
  }
  DCHECK_EQ(synced_details.title(), remote_details.title());

  FileTracker parent_tracker;
  if (!metadata_database()->FindTrackerByTrackerID(
          dirty_tracker_->parent_tracker_id(), &parent_tracker)) {
    token->RecordLog("Missing parent tracker for a non sync-root tracker: "
                     + dirty_tracker_->file_id());
    NOTREACHED();
    SyncCompleted(token.Pass(), SYNC_STATUS_FAILED);
    return;
  }

  if (!HasFolderAsParent(remote_details, parent_tracker.file_id())) {
    // Handle reorganize as deletion + addition.
    token->RecordLog("Detected file reorganize.");
    Prepare(base::Bind(&RemoteToLocalSyncer::DidPrepareForDeletion,
                       weak_ptr_factory_.GetWeakPtr(),
                       SyncCompletedCallback(token.Pass())));
    return;
  }

  if (synced_details.file_kind() == FILE_KIND_FILE) {
    if (synced_details.md5() != remote_details.md5()) {
      token->RecordLog("Detected file content update.");
      HandleContentUpdate(SyncCompletedCallback(token.Pass()));
      return;
    }
  } else {
    DCHECK_EQ(FILE_KIND_FOLDER, synced_details.file_kind());
    if (synced_details.missing()) {
      token->RecordLog("Detected folder update.");
      HandleFolderUpdate(SyncCompletedCallback(token.Pass()));
      return;
    }
    if (dirty_tracker_->needs_folder_listing()) {
      token->RecordLog("Needs listing folder.");
      ListFolderContent(SyncCompletedCallback(token.Pass()));
      return;
    }
    SyncCompleted(token.Pass(), SYNC_STATUS_OK);
    return;
  }

  token->RecordLog("Trivial file change.");
  SyncCompleted(token.Pass(), SYNC_STATUS_OK);
}

void RemoteToLocalSyncer::HandleMissingRemoteMetadata(
    const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);

  drive_service()->GetFileResource(
      dirty_tracker_->file_id(),
      base::Bind(&RemoteToLocalSyncer::DidGetRemoteMetadata,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void RemoteToLocalSyncer::DidGetRemoteMetadata(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::FileResource> entry) {
  DCHECK(sync_context_->GetWorkerTaskRunner()->RunsTasksOnCurrentThread());

  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK &&
      error != google_apis::HTTP_NOT_FOUND) {
    callback.Run(status);
    return;
  }

  if (error == google_apis::HTTP_NOT_FOUND) {
    metadata_database()->UpdateByDeletedRemoteFile(
        dirty_tracker_->file_id(), callback);
    return;
  }

  if (!entry) {
    NOTREACHED();
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  metadata_database()->UpdateByFileResource(
      *entry,
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

void RemoteToLocalSyncer::DidPrepareForAddOrUpdateFile(
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
    sync_action_ = SYNC_ACTION_ADDED;
    // Missing local file case.
    // Download the file and add it to local as a new file.
    DownloadFile(callback);
    return;
  }

  DCHECK(local_changes_->empty() || local_changes_->back().IsAddOrUpdate());
  if (local_changes_->empty()) {
    if (local_metadata_->file_type == SYNC_FILE_TYPE_FILE) {
      sync_action_ = SYNC_ACTION_UPDATED;
      // Download the file and overwrite the existing local file.
      DownloadFile(callback);
      return;
    }

    DCHECK_EQ(SYNC_FILE_TYPE_DIRECTORY, local_metadata_->file_type);

    // Got a remote regular file modification for existing local folder.
    // Our policy prioritize folders in this case.
    // Lower the priority of the tracker to prevent repeated remote sync to the
    // same tracker, and let local-to-remote sync phase process this change.
    metadata_database()->LowerTrackerPriority(dirty_tracker_->tracker_id());
    remote_change_processor()->RecordFakeLocalChange(
        url_,
        FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                   local_metadata_->file_type),
        callback);
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

void RemoteToLocalSyncer::HandleFolderUpdate(
    const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(!remote_metadata_->details().missing());
  DCHECK_EQ(FILE_KIND_FOLDER, remote_metadata_->details().file_kind());

  Prepare(base::Bind(&RemoteToLocalSyncer::DidPrepareForFolderUpdate,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void RemoteToLocalSyncer::DidPrepareForFolderUpdate(
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
    sync_action_ = SYNC_ACTION_ADDED;
    // No local file exists at the path.
    CreateFolder(callback);
    return;
  }

  if (local_metadata_->file_type == SYNC_FILE_TYPE_DIRECTORY) {
    // There already exists a folder, nothing left to do.
    if (dirty_tracker_->needs_folder_listing() &&
        !dirty_tracker_->synced_details().missing()) {
      ListFolderContent(callback);
    } else {
      callback.Run(SYNC_STATUS_OK);
    }
    return;
  }

  DCHECK_EQ(SYNC_FILE_TYPE_FILE, local_metadata_->file_type);
  sync_action_ = SYNC_ACTION_ADDED;
  // Got a remote folder for existing local file.
  // Our policy prioritize folders in this case.
  CreateFolder(callback);
}

void RemoteToLocalSyncer::HandleSyncRootDeletion(
    const SyncStatusCallback& callback) {
  sync_root_deletion_ = true;
  callback.Run(SYNC_STATUS_OK);
}

void RemoteToLocalSyncer::HandleDeletion(
    const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));
  DCHECK(dirty_tracker_->has_synced_details());
  DCHECK(!dirty_tracker_->synced_details().missing());

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(remote_metadata_->details().missing());

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
    sync_action_ = SYNC_ACTION_DELETED;
    DeleteLocalFile(callback);
    return;
  }

  DCHECK(local_changes_->back().IsAddOrUpdate());
  // File is remotely deleted and locally updated.
  // Ignore the remote deletion and handle it as if applied successfully.
  callback.Run(SYNC_STATUS_OK);
}

void RemoteToLocalSyncer::HandleContentUpdate(
    const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));
  DCHECK(dirty_tracker_->has_synced_details());
  DCHECK_EQ(FILE_KIND_FILE, dirty_tracker_->synced_details().file_kind());

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(!remote_metadata_->details().missing());

  DCHECK_NE(dirty_tracker_->synced_details().md5(),
            remote_metadata_->details().md5());

  Prepare(base::Bind(&RemoteToLocalSyncer::DidPrepareForAddOrUpdateFile,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void RemoteToLocalSyncer::ListFolderContent(
    const SyncStatusCallback& callback) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));
  DCHECK(dirty_tracker_->has_synced_details());
  DCHECK(!dirty_tracker_->synced_details().missing());
  DCHECK_EQ(FILE_KIND_FOLDER, dirty_tracker_->synced_details().file_kind());
  DCHECK(dirty_tracker_->needs_folder_listing());

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(!remote_metadata_->details().missing());

  // TODO(tzik): Replace this call with ChildList version.
  drive_service()->GetFileListInDirectory(
      dirty_tracker_->file_id(),
      base::Bind(&RemoteToLocalSyncer::DidListFolderContent,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Passed(make_scoped_ptr(new FileIDList))));
}

void RemoteToLocalSyncer::DidListFolderContent(
    const SyncStatusCallback& callback,
    scoped_ptr<FileIDList> children,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::FileList> file_list) {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  if (!file_list) {
    NOTREACHED();
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  children->reserve(children->size() + file_list->items().size());
  for (ScopedVector<google_apis::FileResource>::const_iterator itr =
           file_list->items().begin();
       itr != file_list->items().end();
       ++itr) {
    children->push_back((*itr)->file_id());
  }

  if (!file_list->next_link().is_empty()) {
    drive_service()->GetRemainingFileList(
        file_list->next_link(),
        base::Bind(&RemoteToLocalSyncer::DidListFolderContent,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback, base::Passed(&children)));
    return;
  }

  metadata_database()->PopulateFolderByChildList(
      dirty_tracker_->file_id(), *children, callback);
}

void RemoteToLocalSyncer::SyncCompleted(scoped_ptr<SyncTaskToken> token,
                                        SyncStatusCode status) {
  token->RecordLog(base::StringPrintf(
      "[Remote -> Local]: Finished: action=%s, tracker=%" PRId64 " status=%s",
      SyncActionToString(sync_action_), dirty_tracker_->tracker_id(),
      SyncStatusCodeToString(status)));

  if (sync_root_deletion_) {
    FinalizeSync(token.Pass(), SYNC_STATUS_OK);
    return;
  }

  if (status == SYNC_STATUS_RETRY) {
    FinalizeSync(token.Pass(), SYNC_STATUS_OK);
    return;
  }

  if (status != SYNC_STATUS_OK) {
    FinalizeSync(token.Pass(), status);
    return;
  }

  DCHECK(dirty_tracker_);
  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());

  FileDetails updated_details = remote_metadata_->details();
  if (!dirty_tracker_->active() ||
      HasDisabledAppRoot(metadata_database(), *dirty_tracker_)) {
    // Operations for an inactive tracker don't update file content.
    if (dirty_tracker_->has_synced_details())
      updated_details.set_md5(dirty_tracker_->synced_details().md5());
    if (!dirty_tracker_->active()) {
      // Keep missing true, as the change hasn't been synced to local.
      updated_details.clear_md5();
      updated_details.set_missing(true);
    }
  }
  metadata_database()->UpdateTracker(
      dirty_tracker_->tracker_id(),
      updated_details,
      base::Bind(&RemoteToLocalSyncer::FinalizeSync,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&token)));
}

void RemoteToLocalSyncer::FinalizeSync(scoped_ptr<SyncTaskToken> token,
                                       SyncStatusCode status) {
  if (prepared_) {
    remote_change_processor()->FinalizeRemoteSync(
        url_, false /* clear_local_change */,
        base::Bind(SyncTaskManager::NotifyTaskDone,
                   base::Passed(&token), status));
    return;
  }

  SyncTaskManager::NotifyTaskDone(token.Pass(), status);
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
  prepared_ = true;

  local_metadata_.reset(new SyncFileMetadata(local_metadata));
  local_changes_.reset(new FileChangeList(local_changes));

  callback.Run(status);
}

void RemoteToLocalSyncer::DeleteLocalFile(const SyncStatusCallback& callback) {
  remote_change_processor()->ApplyRemoteChange(
      FileChange(FileChange::FILE_CHANGE_DELETE, SYNC_FILE_TYPE_UNKNOWN),
      base::FilePath(),
      url_,
      callback);
}

void RemoteToLocalSyncer::DownloadFile(const SyncStatusCallback& callback) {
  DCHECK(sync_context_->GetWorkerTaskRunner()->RunsTasksOnCurrentThread());

  base::Callback<void(webkit_blob::ScopedFile)> did_create_callback =
      base::Bind(&RemoteToLocalSyncer::DidCreateTemporaryFileForDownload,
                 weak_ptr_factory_.GetWeakPtr(), callback);

  sync_context_->GetFileTaskRunner()->PostTask(
      FROM_HERE,
      CreateComposedFunction(
          base::Bind(&CreateTemporaryFile,
                     make_scoped_refptr(sync_context_->GetFileTaskRunner())),
          RelayCallbackToTaskRunner(
              sync_context_->GetWorkerTaskRunner(), FROM_HERE,
              did_create_callback)));
}

void RemoteToLocalSyncer::DidCreateTemporaryFileForDownload(
    const SyncStatusCallback& callback,
    webkit_blob::ScopedFile file) {
  DCHECK(sync_context_->GetWorkerTaskRunner()->RunsTasksOnCurrentThread());
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
  DCHECK(sync_context_->GetWorkerTaskRunner()->RunsTasksOnCurrentThread());

  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  base::FilePath path = file.path();
  base::Callback<void(const std::string&)> did_calculate_callback =
      base::Bind(&RemoteToLocalSyncer::DidCalculateMD5ForDownload,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback, base::Passed(&file));

  sync_context_->GetFileTaskRunner()->PostTask(
      FROM_HERE,
      CreateComposedFunction(
          base::Bind(&drive::util::GetMd5Digest, path),
          RelayCallbackToTaskRunner(
              sync_context_->GetWorkerTaskRunner(), FROM_HERE,
              did_calculate_callback)));
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
  if (status != SYNC_STATUS_OK)
    metadata_database()->LowerTrackerPriority(dirty_tracker_->tracker_id());
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

SyncStatusCallback RemoteToLocalSyncer::SyncCompletedCallback(
    scoped_ptr<SyncTaskToken> token) {
  return base::Bind(&RemoteToLocalSyncer::SyncCompleted,
                    weak_ptr_factory_.GetWeakPtr(),
                    base::Passed(&token));
}

}  // namespace drive_backend
}  // namespace sync_file_system
