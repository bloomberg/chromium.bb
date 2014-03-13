// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/drive/drive_api_service.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

////////////////////////////////////////////////////////////////////////////////
// Functions below are for wrapping the access to legacy GData WAPI classes.

bool IsDeleted(const google_apis::ResourceEntry& entry) {
  return entry.deleted();
}

bool HasNoParents(const google_apis::ResourceEntry& entry) {
  return !entry.GetLinkByType(google_apis::Link::LINK_PARENT);
}

bool HasFolderAsParent(const google_apis::ResourceEntry& entry,
                       const std::string& parent_id) {
  const ScopedVector<google_apis::Link>& links = entry.links();
  for (ScopedVector<google_apis::Link>::const_iterator itr = links.begin();
       itr != links.end(); ++itr) {
    const google_apis::Link& link = **itr;
    if (link.type() != google_apis::Link::LINK_PARENT)
      continue;
    if (drive::util::ExtractResourceIdFromUrl(link.href()) == parent_id)
      return true;
  }
  return false;
}

bool LessOnCreationTime(const google_apis::ResourceEntry& left,
                        const google_apis::ResourceEntry& right) {
  return left.published_time() < right.published_time();
}

// Posts a request to continue listing.  Returns false if the list doesn't need
// listing anymore.
bool GetRemainingFileList(
    google_apis::CancelCallback* cancel_callback,
    drive::DriveServiceInterface* api_service,
    const google_apis::ResourceList& resource_list,
    const google_apis::GetResourceListCallback& callback) {
  GURL next_url;
  if (!resource_list.GetNextFeedURL(&next_url))
    return false;

  *cancel_callback = api_service->GetRemainingFileList(next_url, callback);
  return true;
}

std::string GetID(const google_apis::ResourceEntry& entry) {
  return entry.resource_id();
}

ScopedVector<google_apis::FileResource> ConvertResourceEntriesToFileResources(
    const ScopedVector<google_apis::ResourceEntry>& entries) {
  ScopedVector<google_apis::FileResource> resources;
  for (ScopedVector<google_apis::ResourceEntry>::const_iterator itr =
           entries.begin();
       itr != entries.end();
       ++itr) {
    resources.push_back(
        drive::util::ConvertResourceEntryToFileResource(
            **itr).release());
  }
  return resources.Pass();
}

// Functions above are for wrapping the access to legacy GData WAPI classes.
////////////////////////////////////////////////////////////////////////////////

}  // namespace

SyncEngineInitializer::SyncEngineInitializer(
    SyncEngineContext* sync_context,
    base::SequencedTaskRunner* task_runner,
    drive::DriveServiceInterface* drive_service,
    const base::FilePath& database_path,
    leveldb::Env* env_override)
    : sync_context_(sync_context),
      env_override_(env_override),
      task_runner_(task_runner),
      drive_service_(drive_service),
      database_path_(database_path),
      find_sync_root_retry_count_(0),
      largest_change_id_(0),
      weak_ptr_factory_(this) {
  DCHECK(task_runner);
  DCHECK(drive_service_);
}

SyncEngineInitializer::~SyncEngineInitializer() {
  if (!cancel_callback_.is_null())
    cancel_callback_.Run();
}

void SyncEngineInitializer::RunSequential(const SyncStatusCallback& callback) {
  util::Log(logging::LOG_VERBOSE, FROM_HERE, "[Initialize] Start.");

  // The metadata seems to have been already initialized. Just return with OK.
  if (sync_context_ && sync_context_->GetMetadataDatabase()) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Already initialized.");
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  MetadataDatabase::Create(
      task_runner_.get(), database_path_, env_override_,
      base::Bind(&SyncEngineInitializer::DidCreateMetadataDatabase,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

scoped_ptr<MetadataDatabase> SyncEngineInitializer::PassMetadataDatabase() {
  return metadata_database_.Pass();
}

void SyncEngineInitializer::DidCreateMetadataDatabase(
    const SyncStatusCallback& callback,
    SyncStatusCode status,
    scoped_ptr<MetadataDatabase> instance) {
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to initialize MetadataDatabase.");
    callback.Run(status);
    return;
  }

  DCHECK(instance);
  metadata_database_ = instance.Pass();
  if (metadata_database_->HasSyncRoot()) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Found local cache of sync-root.");
    callback.Run(SYNC_STATUS_OK);
    return;
  }

  GetAboutResource(callback);
}

void SyncEngineInitializer::GetAboutResource(
    const SyncStatusCallback& callback) {
  set_used_network(true);
  drive_service_->GetAboutResource(
      base::Bind(&SyncEngineInitializer::DidGetAboutResource,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void SyncEngineInitializer::DidGetAboutResource(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  cancel_callback_.Reset();

  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to get AboutResource.");
    callback.Run(status);
    return;
  }

  DCHECK(about_resource);
  root_folder_id_ = about_resource->root_folder_id();
  largest_change_id_ = about_resource->largest_change_id();

  DCHECK(!root_folder_id_.empty());
  FindSyncRoot(callback);
}

void SyncEngineInitializer::FindSyncRoot(const SyncStatusCallback& callback) {
  if (find_sync_root_retry_count_++ >= kMaxRetry) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Reached max retry count.");
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  set_used_network(true);
  cancel_callback_ = drive_service_->SearchByTitle(
      kSyncRootFolderTitle,
      std::string(),  // parent_folder_id
      base::Bind(&SyncEngineInitializer::DidFindSyncRoot,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void SyncEngineInitializer::DidFindSyncRoot(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  cancel_callback_.Reset();

  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to find sync root.");
    callback.Run(status);
    return;
  }

  if (!resource_list) {
    NOTREACHED();
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Got invalid resource list.");
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  ScopedVector<google_apis::ResourceEntry>* entries =
      resource_list->mutable_entries();
  for (ScopedVector<google_apis::ResourceEntry>::iterator itr =
           entries->begin();
       itr != entries->end(); ++itr) {
    google_apis::ResourceEntry* entry = *itr;

    // Ignore deleted folder.
    if (IsDeleted(*entry))
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
  if (GetRemainingFileList(
          &cancel_callback_,
          drive_service_, *resource_list,
          base::Bind(&SyncEngineInitializer::DidFindSyncRoot,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback)))
    return;

  if (!sync_root_folder_) {
    CreateSyncRoot(callback);
    return;
  }

  if (!HasNoParents(*sync_root_folder_)) {
    DetachSyncRoot(callback);
    return;
  }

  ListAppRootFolders(callback);
}

void SyncEngineInitializer::CreateSyncRoot(const SyncStatusCallback& callback) {
  DCHECK(!sync_root_folder_);
  set_used_network(true);
  cancel_callback_ = drive_service_->AddNewDirectory(
      root_folder_id_, kSyncRootFolderTitle,
      drive::DriveServiceInterface::AddNewDirectoryOptions(),
      base::Bind(&SyncEngineInitializer::DidCreateSyncRoot,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void SyncEngineInitializer::DidCreateSyncRoot(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(!sync_root_folder_);
  cancel_callback_.Reset();

  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to create sync root.");
    callback.Run(status);
    return;
  }

  FindSyncRoot(callback);
}

void SyncEngineInitializer::DetachSyncRoot(const SyncStatusCallback& callback) {
  DCHECK(sync_root_folder_);
  set_used_network(true);
  cancel_callback_ = drive_service_->RemoveResourceFromDirectory(
      root_folder_id_, GetID(*sync_root_folder_),
      base::Bind(&SyncEngineInitializer::DidDetachSyncRoot,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void SyncEngineInitializer::DidDetachSyncRoot(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error) {
  cancel_callback_.Reset();

  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to detach sync root.");
    callback.Run(status);
    return;
  }

  ListAppRootFolders(callback);
}

void SyncEngineInitializer::ListAppRootFolders(
    const SyncStatusCallback& callback) {
  DCHECK(sync_root_folder_);
  set_used_network(true);
  cancel_callback_ = drive_service_->GetResourceListInDirectory(
      GetID(*sync_root_folder_),
      base::Bind(&SyncEngineInitializer::DidListAppRootFolders,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void SyncEngineInitializer::DidListAppRootFolders(
    const SyncStatusCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  cancel_callback_.Reset();

  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to get initial app-root folders.");
    callback.Run(status);
    return;
  }

  if (!resource_list) {
    NOTREACHED();
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Got invalid initial app-root list.");
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  ScopedVector<google_apis::ResourceEntry>* new_entries =
      resource_list->mutable_entries();
  app_root_folders_.insert(app_root_folders_.end(),
                           new_entries->begin(), new_entries->end());
  new_entries->weak_clear();

  set_used_network(true);
  if (GetRemainingFileList(
          &cancel_callback_,
          drive_service_,
          *resource_list,
          base::Bind(&SyncEngineInitializer::DidListAppRootFolders,
                     weak_ptr_factory_.GetWeakPtr(), callback)))
    return;

  PopulateDatabase(callback);
}

void SyncEngineInitializer::PopulateDatabase(
    const SyncStatusCallback& callback) {
  DCHECK(sync_root_folder_);
  metadata_database_->PopulateInitialData(
      largest_change_id_,
      *drive::util::ConvertResourceEntryToFileResource(
          *sync_root_folder_),
      ConvertResourceEntriesToFileResources(app_root_folders_),
      base::Bind(&SyncEngineInitializer::DidPopulateDatabase,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void SyncEngineInitializer::DidPopulateDatabase(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to populate initial data"
              " to MetadataDatabase.");
    callback.Run(status);
    return;
  }

  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "[Initialize] Completed successfully.");
  callback.Run(SYNC_STATUS_OK);
}

}  // namespace drive_backend
}  // namespace sync_file_system
