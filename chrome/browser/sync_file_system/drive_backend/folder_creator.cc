// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/folder_creator.h"

#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_util.h"
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
      base::Bind(&FolderCreator::DidCreateFolder,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void FolderCreator::DidCreateFolder(
    const FileIDCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_CREATED) {
    callback.Run(std::string(), GDataErrorCodeToSyncStatusCode(error));
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
  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(std::string(), GDataErrorCodeToSyncStatusCode(error));
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
  std::string file_id = oldest->resource_id();

  metadata_database_->UpdateByFileResourceList(
      files.Pass(), base::Bind(callback, file_id));
}

}  // namespace drive_backend
}  // namespace sync_file_system
