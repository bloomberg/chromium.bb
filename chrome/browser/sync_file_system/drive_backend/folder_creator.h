// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FOLDER_CREATOR_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FOLDER_CREATOR_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "google_apis/drive/gdata_errorcode.h"

namespace drive {
class DriveServiceInterface;
}

namespace google_apis {
class FileList;
class FileResource;
class ResourceEntry;
}

namespace sync_file_system {
namespace drive_backend {

class MetadataDatabase;
class SyncEngineContext;

class FolderCreator {
 public:
  typedef base::Callback<void(const std::string& file_id,
                              SyncStatusCode status)> FileIDCallback;

  FolderCreator(drive::DriveServiceInterface* drive_service,
                MetadataDatabase* metadata_database,
                const std::string& parent_folder_id,
                const std::string& title);
  ~FolderCreator();

  void Run(const FileIDCallback& callback);

 private:
  void DidCreateFolder(const FileIDCallback& callback,
                       google_apis::GDataErrorCode error,
                       scoped_ptr<google_apis::FileResource> entry);
  void DidListFolders(const FileIDCallback& callback,
                      ScopedVector<google_apis::FileResource> candidates,
                      google_apis::GDataErrorCode error,
                      scoped_ptr<google_apis::FileList> file_list);

  drive::DriveServiceInterface* drive_service_;
  MetadataDatabase* metadata_database_;

  const std::string parent_folder_id_;
  const std::string title_;

  base::WeakPtrFactory<FolderCreator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FolderCreator);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FOLDER_CREATOR_H_
