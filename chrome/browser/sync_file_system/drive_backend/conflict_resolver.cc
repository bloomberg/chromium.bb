// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/conflict_resolver.h"

#include "base/callback.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "google_apis/drive/drive_api_parser.h"

namespace sync_file_system {
namespace drive_backend {

ConflictResolver::ConflictResolver(SyncEngineContext* sync_context)
    : sync_context_(sync_context),
      weak_ptr_factory_(this) {}

ConflictResolver::~ConflictResolver() {}

void ConflictResolver::RunPreflight(scoped_ptr<SyncTaskToken> token) {
  token->InitializeTaskLog("Conflict Resolution");

  scoped_ptr<BlockingFactor> blocking_factor(new BlockingFactor);
  blocking_factor->exclusive = true;
  SyncTaskManager::UpdateBlockingFactor(
      token.Pass(), blocking_factor.Pass(),
      base::Bind(&ConflictResolver::RunExclusive,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ConflictResolver::RunExclusive(scoped_ptr<SyncTaskToken> token) {
  if (!IsContextReady()) {
    SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_FAILED);
    return;
  }

  // Conflict resolution should be invoked on clean tree.
  if (metadata_database()->HasDirtyTracker()) {
    NOTREACHED();
    SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_FAILED);
    return;
  }

  TrackerIDSet trackers;
  if (metadata_database()->GetMultiParentFileTrackers(
          &target_file_id_, &trackers)) {
    DCHECK_LT(1u, trackers.size());
    if (!trackers.has_active()) {
      NOTREACHED();
      SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_FAILED);
      return;
    }

    token->RecordLog(base::StringPrintf(
        "Detected multi-parent trackers (active tracker_id=%" PRId64 ")",
        trackers.active_tracker()));

    DCHECK(trackers.has_active());
    for (TrackerIDSet::const_iterator itr = trackers.begin();
         itr != trackers.end(); ++itr) {
      FileTracker tracker;
      if (!metadata_database()->FindTrackerByTrackerID(*itr, &tracker)) {
        NOTREACHED();
        continue;
      }

      if (tracker.active())
        continue;

      FileTracker parent_tracker;
      bool should_success = metadata_database()->FindTrackerByTrackerID(
          tracker.parent_tracker_id(), &parent_tracker);
      if (!should_success) {
        NOTREACHED();
        SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_FAILED);
        return;
      }
      parents_to_remove_.push_back(parent_tracker.file_id());
    }
    DetachFromNonPrimaryParents(token.Pass());
    return;
  }

  if (metadata_database()->GetConflictingTrackers(&trackers)) {
    target_file_id_ = PickPrimaryFile(trackers);
    DCHECK(!target_file_id_.empty());
    int64 primary_tracker_id = -1;
    for (TrackerIDSet::const_iterator itr = trackers.begin();
         itr != trackers.end(); ++itr) {
      FileTracker tracker;
      if (!metadata_database()->FindTrackerByTrackerID(*itr, &tracker)) {
        NOTREACHED();
        continue;
      }
      if (tracker.file_id() != target_file_id_) {
        non_primary_file_ids_.push_back(
            std::make_pair(tracker.file_id(), tracker.synced_details().etag()));
      } else {
        primary_tracker_id = tracker.tracker_id();
      }
    }

    token->RecordLog(base::StringPrintf(
        "Detected %" PRIuS " conflicting trackers "
        "(primary tracker_id=%" PRId64 ")",
        non_primary_file_ids_.size(), primary_tracker_id));

    RemoveNonPrimaryFiles(token.Pass());
    return;
  }

  SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_NO_CONFLICT);
}

void ConflictResolver::DetachFromNonPrimaryParents(
    scoped_ptr<SyncTaskToken> token) {
  DCHECK(!parents_to_remove_.empty());

  // TODO(tzik): Check if ETag match is available for
  // RemoteResourceFromDirectory.
  std::string parent_folder_id = parents_to_remove_.back();
  parents_to_remove_.pop_back();

  token->RecordLog(base::StringPrintf(
      "Detach %s from %s",
      target_file_id_.c_str(), parent_folder_id.c_str()));

  drive_service()->RemoveResourceFromDirectory(
      parent_folder_id, target_file_id_,
      base::Bind(&ConflictResolver::DidDetachFromParent,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&token)));
}

void ConflictResolver::DidDetachFromParent(scoped_ptr<SyncTaskToken> token,
                                           google_apis::GDataErrorCode error) {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    SyncTaskManager::NotifyTaskDone(token.Pass(), status);
    return;
  }

  if (!parents_to_remove_.empty()) {
    DetachFromNonPrimaryParents(token.Pass());
    return;
  }

  SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_OK);
}

std::string ConflictResolver::PickPrimaryFile(const TrackerIDSet& trackers) {
  scoped_ptr<FileMetadata> primary;
  for (TrackerIDSet::const_iterator itr = trackers.begin();
       itr != trackers.end(); ++itr) {
    FileTracker tracker;
    if (!metadata_database()->FindTrackerByTrackerID(*itr, &tracker)) {
      NOTREACHED();
      continue;
    }

    scoped_ptr<FileMetadata> file_metadata(new FileMetadata);
    if (!metadata_database()->FindFileByFileID(
            tracker.file_id(), file_metadata.get())) {
      NOTREACHED();
      continue;
    }

    if (!primary) {
      primary = file_metadata.Pass();
      continue;
    }

    DCHECK(primary->details().file_kind() == FILE_KIND_FILE ||
           primary->details().file_kind() == FILE_KIND_FOLDER);
    DCHECK(file_metadata->details().file_kind() == FILE_KIND_FILE ||
           file_metadata->details().file_kind() == FILE_KIND_FOLDER);

    if (primary->details().file_kind() == FILE_KIND_FILE) {
      if (file_metadata->details().file_kind() == FILE_KIND_FOLDER) {
        // Prioritize folders over regular files.
        primary = file_metadata.Pass();
        continue;
      }

      DCHECK(file_metadata->details().file_kind() == FILE_KIND_FILE);
      if (primary->details().modification_time() <
          file_metadata->details().modification_time()) {
        // Prioritize last write for regular files.
        primary = file_metadata.Pass();
        continue;
      }

      continue;
    }

    DCHECK(primary->details().file_kind() == FILE_KIND_FOLDER);
    if (file_metadata->details().file_kind() == FILE_KIND_FILE) {
      // Prioritize folders over regular files.
      continue;
    }

    DCHECK(file_metadata->details().file_kind() == FILE_KIND_FOLDER);
    if (primary->details().creation_time() >
        file_metadata->details().creation_time()) {
      // Prioritize first create for folders.
      primary = file_metadata.Pass();
      continue;
    }
  }

  if (primary)
    return primary->file_id();
  return std::string();
}

void ConflictResolver::RemoveNonPrimaryFiles(scoped_ptr<SyncTaskToken> token) {
  DCHECK(!non_primary_file_ids_.empty());

  std::string file_id = non_primary_file_ids_.back().first;
  std::string etag = non_primary_file_ids_.back().second;
  non_primary_file_ids_.pop_back();

  DCHECK_NE(target_file_id_, file_id);

  token->RecordLog(base::StringPrintf(
      "Remove non-primary file %s", file_id.c_str()));

  // TODO(tzik): Check if the file is a folder, and merge its contents into
  // the folder identified by |target_file_id_|.
  drive_service()->DeleteResource(
      file_id, etag,
      base::Bind(&ConflictResolver::DidRemoveFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&token), file_id));
}

void ConflictResolver::DidRemoveFile(scoped_ptr<SyncTaskToken> token,
                                     const std::string& file_id,
                                     google_apis::GDataErrorCode error) {
  if (error == google_apis::HTTP_PRECONDITION ||
      error == google_apis::HTTP_CONFLICT) {
    UpdateFileMetadata(file_id, token.Pass());
    return;
  }

  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK && error != google_apis::HTTP_NOT_FOUND) {
    SyncTaskManager::NotifyTaskDone(token.Pass(), status);
    return;
  }

  deleted_file_ids_.push_back(file_id);
  if (!non_primary_file_ids_.empty()) {
    RemoveNonPrimaryFiles(token.Pass());
    return;
  }

  metadata_database()->UpdateByDeletedRemoteFileList(
      deleted_file_ids_, SyncTaskToken::WrapToCallback(token.Pass()));
}

bool ConflictResolver::IsContextReady() {
  return sync_context_->GetDriveService() &&
      sync_context_->GetMetadataDatabase();
}

void ConflictResolver::UpdateFileMetadata(
    const std::string& file_id,
    scoped_ptr<SyncTaskToken> token) {
  drive_service()->GetFileResource(
      file_id,
      base::Bind(&ConflictResolver::DidGetRemoteMetadata,
                 weak_ptr_factory_.GetWeakPtr(), file_id,
                 base::Passed(&token)));
}

void ConflictResolver::DidGetRemoteMetadata(
    const std::string& file_id,
    scoped_ptr<SyncTaskToken> token,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::FileResource> entry) {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK && error != google_apis::HTTP_NOT_FOUND) {
    SyncTaskManager::NotifyTaskDone(token.Pass(), status);
    return;
  }

  if (error != google_apis::HTTP_NOT_FOUND) {
    metadata_database()->UpdateByDeletedRemoteFile(
        file_id, SyncTaskToken::WrapToCallback(token.Pass()));
    return;
  }

  if (!entry) {
    NOTREACHED();
    SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_FAILED);
    return;
  }

  metadata_database()->UpdateByFileResource(
      *entry, SyncTaskToken::WrapToCallback(token.Pass()));
}

drive::DriveServiceInterface* ConflictResolver::drive_service() {
  set_used_network(true);
  return sync_context_->GetDriveService();
}

MetadataDatabase* ConflictResolver::metadata_database() {
  return sync_context_->GetMetadataDatabase();
}

}  // namespace drive_backend
}  // namespace sync_file_system
