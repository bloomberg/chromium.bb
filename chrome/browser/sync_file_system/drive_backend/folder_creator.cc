// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/folder_creator.h"

#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"

namespace drive {
class DriveServiceInterface;
class DriveUploaderInterface;
}

namespace google_apis {
class ResourceEntry;
class ResourceList;
}

namespace sync_file_system {
namespace drive_backend {

FolderCreator::FolderCreator(drive::DriveServiceInterface* drive_service,
                             MetadataDatabase* metadata_database,
                             const std::string& parent_folder_id,
                             const std::string& title)
    : drive_service_(drive_service),
      metadata_database_(metadata_database),
      parent_folder_id_(parent_folder_id),
      title_(title),
      weak_ptr_factory_(this) {
}

FolderCreator::~FolderCreator() {
}

void FolderCreator::Run(const FileIDCallback& callback) {
  drive_service_->AddNewDirectory(
      parent_folder_id_,
      title_,
      drive::DriveServiceInterface::AddNewDirectoryOptions(),
      base::Bind(&FolderCreator::DidCreateFolder,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void FolderCreator::DidCreateFolder(
    const FileIDCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    callback.Run(std::string(), status);
    return;
  }

  drive_service_->SearchByTitle(
      title_, parent_folder_id_,
      base::Bind(&FolderCreator::DidListFolders,
                 weak_ptr_factory_.GetWeakPtr(), callback,
                 base::Passed(ScopedVector<google_apis::ResourceEntry>())));
}

void FolderCreator::DidListFolders(
    const FileIDCallback& callback,
    ScopedVector<google_apis::ResourceEntry> candidates,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    callback.Run(std::string(), status);
    return;
  }

  if (!resource_list) {
    NOTREACHED();
    callback.Run(std::string(), SYNC_STATUS_FAILED);
    return;
  }

  candidates.reserve(candidates.size() + resource_list->entries().size());
  candidates.insert(candidates.end(),
                    resource_list->entries().begin(),
                    resource_list->entries().end());
  resource_list->mutable_entries()->weak_clear();

  GURL next_feed;
  if (resource_list->GetNextFeedURL(&next_feed)) {
    drive_service_->GetRemainingFileList(
        next_feed,
        base::Bind(&FolderCreator::DidListFolders,
                   weak_ptr_factory_.GetWeakPtr(), callback,
                   base::Passed(&candidates)));
    return;
  }

  ScopedVector<google_apis::FileResource> files;
  files.reserve(candidates.size());
  for (size_t i = 0; i < candidates.size(); ++i) {
    files.push_back(drive::util::ConvertResourceEntryToFileResource(
        *candidates[i]).release());
  }

  scoped_ptr<google_apis::ResourceEntry> oldest =
      GetOldestCreatedFolderResource(candidates.Pass());

  if (!oldest) {
    callback.Run(std::string(), SYNC_FILE_ERROR_NOT_FOUND);
    return;
  }

  std::string file_id = oldest->resource_id();

  metadata_database_->UpdateByFileResourceList(
      files.Pass(), base::Bind(&FolderCreator::DidUpdateDatabase,
                               weak_ptr_factory_.GetWeakPtr(),
                               file_id, callback));
}

void FolderCreator::DidUpdateDatabase(const std::string& file_id,
                                      const FileIDCallback& callback,
                                      SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(std::string(), status);
    return;
  }

  DCHECK(!file_id.empty());
  if (!metadata_database_->FindFileByFileID(file_id, NULL)) {
    callback.Run(std::string(), SYNC_FILE_ERROR_NOT_FOUND);
    return;
  }

  callback.Run(file_id, status);
}

}  // namespace drive_backend
}  // namespace sync_file_system
