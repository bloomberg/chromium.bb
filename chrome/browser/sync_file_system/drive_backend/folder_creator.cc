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
    scoped_ptr<google_apis::FileResource> entry) {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    callback.Run(std::string(), status);
    return;
  }

  drive_service_->SearchByTitle(
      title_, parent_folder_id_,
      base::Bind(&FolderCreator::DidListFolders,
                 weak_ptr_factory_.GetWeakPtr(), callback,
                 base::Passed(ScopedVector<google_apis::FileResource>())));
}

void FolderCreator::DidListFolders(
    const FileIDCallback& callback,
    ScopedVector<google_apis::FileResource> candidates,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::FileList> file_list) {
  SyncStatusCode status = GDataErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    callback.Run(std::string(), status);
    return;
  }

  if (!file_list) {
    NOTREACHED();
    callback.Run(std::string(), SYNC_STATUS_FAILED);
    return;
  }

  candidates.reserve(candidates.size() + file_list->items().size());
  candidates.insert(candidates.end(),
                    file_list->items().begin(),
                    file_list->items().end());
  file_list->mutable_items()->weak_clear();

  if (!file_list->next_link().is_empty()) {
    drive_service_->GetRemainingFileList(
        file_list->next_link(),
        base::Bind(&FolderCreator::DidListFolders,
                   weak_ptr_factory_.GetWeakPtr(), callback,
                   base::Passed(&candidates)));
    return;
  }

  const google_apis::FileResource* oldest = NULL;
  for (size_t i = 0; i < candidates.size(); ++i) {
    const google_apis::FileResource& entry = *candidates[i];
    if (!entry.IsDirectory() || entry.labels().is_trashed())
      continue;

    if (!oldest || oldest->created_date() > entry.created_date())
      oldest = &entry;
  }

  if (!oldest) {
    callback.Run(std::string(), SYNC_FILE_ERROR_NOT_FOUND);
    return;
  }

  std::string file_id = oldest->file_id();

  status = metadata_database_->UpdateByFileResourceList(candidates.Pass());
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
