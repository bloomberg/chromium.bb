// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/drive/drive_api_service.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

////////////////////////////////////////////////////////////////////////////////
// Functions below are for wrapping the access to legacy GData WAPI classes.

bool HasNoParents(const google_apis::FileResource& entry) {
  return entry.parents().empty();
}

bool HasFolderAsParent(const google_apis::FileResource& entry,
                       const std::string& parent_id) {
  for (size_t i = 0; i < entry.parents().size(); ++i) {
    if (entry.parents()[i].file_id() == parent_id)
      return true;
  }
  return false;
}

bool LessOnCreationTime(const google_apis::FileResource& left,
                        const google_apis::FileResource& right) {
  return left.created_date() < right.created_date();
}

typedef base::Callback<void(scoped_ptr<SyncTaskToken> token,
                            google_apis::GDataErrorCode error,
                            scoped_ptr<google_apis::ResourceList> resources)>
    TokenAndResourceListCallback;

// Functions above are for wrapping the access to legacy GData WAPI classes.
////////////////////////////////////////////////////////////////////////////////

}  // namespace

SyncEngineInitializer::SyncEngineInitializer(
    SyncEngineContext* sync_context,
    const base::FilePath& database_path,
    leveldb::Env* env_override)
    : sync_context_(sync_context),
      env_override_(env_override),
      database_path_(database_path),
      find_sync_root_retry_count_(0),
      largest_change_id_(0),
      weak_ptr_factory_(this) {
  DCHECK(sync_context);
}

SyncEngineInitializer::~SyncEngineInitializer() {
  if (!cancel_callback_.is_null())
    cancel_callback_.Run();
}

void SyncEngineInitializer::RunPreflight(scoped_ptr<SyncTaskToken> token) {
  util::Log(logging::LOG_VERBOSE, FROM_HERE, "[Initialize] Start.");
  DCHECK(sync_context_);
  DCHECK(sync_context_->GetDriveService());

  // The metadata seems to have been already initialized. Just return with OK.
  if (sync_context_->GetMetadataDatabase()) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Already initialized.");
    SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_OK);
    return;
  }

  SyncStatusCode status = SYNC_STATUS_FAILED;
  scoped_ptr<MetadataDatabase> metadata_database =
      MetadataDatabase::Create(database_path_, env_override_, &status);

  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to initialize MetadataDatabase.");
    SyncTaskManager::NotifyTaskDone(token.Pass(), status);
    return;
  }

  DCHECK(metadata_database);
  metadata_database_ = metadata_database.Pass();
  if (metadata_database_->HasSyncRoot()) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Found local cache of sync-root.");
    SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_OK);
    return;
  }

  GetAboutResource(token.Pass());
}

scoped_ptr<MetadataDatabase> SyncEngineInitializer::PassMetadataDatabase() {
  return metadata_database_.Pass();
}

void SyncEngineInitializer::GetAboutResource(
    scoped_ptr<SyncTaskToken> token) {
  set_used_network(true);
  sync_context_->GetDriveService()->GetAboutResource(
      base::Bind(&SyncEngineInitializer::DidGetAboutResource,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&token)));
}

void SyncEngineInitializer::DidGetAboutResource(
    scoped_ptr<SyncTaskToken> token,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  cancel_callback_.Reset();

  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to get AboutResource.");
    SyncTaskManager::NotifyTaskDone(token.Pass(), status);
    return;
  }

  DCHECK(about_resource);
  root_folder_id_ = about_resource->root_folder_id();
  largest_change_id_ = about_resource->largest_change_id();

  DCHECK(!root_folder_id_.empty());
  FindSyncRoot(token.Pass());
}

void SyncEngineInitializer::FindSyncRoot(scoped_ptr<SyncTaskToken> token) {
  if (find_sync_root_retry_count_++ >= kMaxRetry) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Reached max retry count.");
    SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_FAILED);
    return;
  }

  set_used_network(true);
  cancel_callback_ = sync_context_->GetDriveService()->SearchByTitle(
      kSyncRootFolderTitle,
      std::string(),  // parent_folder_id
      base::Bind(&SyncEngineInitializer::DidFindSyncRoot,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&token)));
}

void SyncEngineInitializer::DidFindSyncRoot(
    scoped_ptr<SyncTaskToken> token,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::FileList> file_list) {
  cancel_callback_.Reset();

  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to find sync root.");
    SyncTaskManager::NotifyTaskDone(token.Pass(), status);
    return;
  }

  if (!file_list) {
    NOTREACHED();
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Got invalid resource list.");
    SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_FAILED);
    return;
  }

  ScopedVector<google_apis::FileResource>* items = file_list->mutable_items();
  for (ScopedVector<google_apis::FileResource>::iterator itr = items->begin();
       itr != items->end(); ++itr) {
    google_apis::FileResource* entry = *itr;

    // Ignore deleted folder.
    if (entry->labels().is_trashed())
      continue;

    // Pick an orphaned folder or a direct child of the root folder and
    // ignore others.
    DCHECK(!root_folder_id_.empty());
    if (!HasNoParents(*entry) && !HasFolderAsParent(*entry, root_folder_id_))
      continue;

    if (!sync_root_folder_ || LessOnCreationTime(*entry, *sync_root_folder_)) {
      sync_root_folder_.reset(entry);
      *itr = NULL;
    }
  }

  set_used_network(true);
  // If there are more results, retrieve them.
  if (!file_list->next_link().is_empty()) {
    cancel_callback_ = sync_context_->GetDriveService()->GetRemainingFileList(
        file_list->next_link(),
        base::Bind(&SyncEngineInitializer::DidFindSyncRoot,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(&token)));
    return;
  }

  if (!sync_root_folder_) {
    CreateSyncRoot(token.Pass());
    return;
  }

  if (!HasNoParents(*sync_root_folder_)) {
    DetachSyncRoot(token.Pass());
    return;
  }

  ListAppRootFolders(token.Pass());
}

void SyncEngineInitializer::CreateSyncRoot(scoped_ptr<SyncTaskToken> token) {
  DCHECK(!sync_root_folder_);
  set_used_network(true);
  cancel_callback_ = sync_context_->GetDriveService()->AddNewDirectory(
      root_folder_id_, kSyncRootFolderTitle,
      drive::DriveServiceInterface::AddNewDirectoryOptions(),
      base::Bind(&SyncEngineInitializer::DidCreateSyncRoot,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&token)));
}

void SyncEngineInitializer::DidCreateSyncRoot(
    scoped_ptr<SyncTaskToken> token,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::FileResource> entry) {
  DCHECK(!sync_root_folder_);
  cancel_callback_.Reset();

  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to create sync root.");
    SyncTaskManager::NotifyTaskDone(token.Pass(), status);
    return;
  }

  FindSyncRoot(token.Pass());
}

void SyncEngineInitializer::DetachSyncRoot(scoped_ptr<SyncTaskToken> token) {
  DCHECK(sync_root_folder_);
  set_used_network(true);
  cancel_callback_ =
      sync_context_->GetDriveService()->RemoveResourceFromDirectory(
          root_folder_id_,
          sync_root_folder_->file_id(),
          base::Bind(&SyncEngineInitializer::DidDetachSyncRoot,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&token)));
}

void SyncEngineInitializer::DidDetachSyncRoot(
    scoped_ptr<SyncTaskToken> token,
    google_apis::GDataErrorCode error) {
  cancel_callback_.Reset();

  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to detach sync root.");
    SyncTaskManager::NotifyTaskDone(token.Pass(), status);
    return;
  }

  ListAppRootFolders(token.Pass());
}

void SyncEngineInitializer::ListAppRootFolders(
    scoped_ptr<SyncTaskToken> token) {
  DCHECK(sync_root_folder_);
  set_used_network(true);
  cancel_callback_ =
      sync_context_->GetDriveService()->GetFileListInDirectory(
          sync_root_folder_->file_id(),
          base::Bind(&SyncEngineInitializer::DidListAppRootFolders,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&token)));
}

void SyncEngineInitializer::DidListAppRootFolders(
    scoped_ptr<SyncTaskToken> token,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::FileList> file_list) {
  cancel_callback_.Reset();

  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to get initial app-root folders.");
    SyncTaskManager::NotifyTaskDone(token.Pass(), status);
    return;
  }

  if (!file_list) {
    NOTREACHED();
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Got invalid initial app-root list.");
    SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_FAILED);
    return;
  }

  ScopedVector<google_apis::FileResource>* new_entries =
      file_list->mutable_items();
  app_root_folders_.insert(app_root_folders_.end(),
                           new_entries->begin(), new_entries->end());
  new_entries->weak_clear();

  set_used_network(true);
  if (!file_list->next_link().is_empty()) {
    cancel_callback_ =
        sync_context_->GetDriveService()->GetRemainingFileList(
            file_list->next_link(),
            base::Bind(&SyncEngineInitializer::DidListAppRootFolders,
                       weak_ptr_factory_.GetWeakPtr(), base::Passed(&token)));
    return;
  }

  PopulateDatabase(token.Pass());
}

void SyncEngineInitializer::PopulateDatabase(
    scoped_ptr<SyncTaskToken> token) {
  DCHECK(sync_root_folder_);
  SyncStatusCode status = metadata_database_->PopulateInitialData(
      largest_change_id_, *sync_root_folder_, app_root_folders_);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to populate initial data"
              " to MetadataDatabase.");
    SyncTaskManager::NotifyTaskDone(token.Pass(), status);
    return;
  }

  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "[Initialize] Completed successfully.");
  SyncTaskManager::NotifyTaskDone(token.Pass(), SYNC_STATUS_OK);
}

}  // namespace drive_backend
}  // namespace sync_file_system
