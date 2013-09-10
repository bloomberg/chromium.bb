// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_FAKE_DRIVE_SERVICE_HELPER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_FAKE_DRIVE_SERVICE_HELPER_H_

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"

namespace base {
class FilePath;
}

namespace sync_file_system {
namespace drive_backend {

class FakeDriveServiceHelper {
 public:
  FakeDriveServiceHelper(drive::FakeDriveService* fake_drive_service,
                         drive::DriveUploaderInterface* drive_uploader);
  virtual ~FakeDriveServiceHelper();

  google_apis::GDataErrorCode AddOrphanedFolder(
      const std::string& title,
      std::string* folder_id);
  google_apis::GDataErrorCode AddFolder(
      const std::string& parent_folder_id,
      const std::string& title,
      std::string* folder_id);
  google_apis::GDataErrorCode AddFile(
      const std::string& parent_folder_id,
      const std::string& title,
      const std::string& content,
      std::string* file_id);
  google_apis::GDataErrorCode UpdateFile(
      const std::string& file_id,
      const std::string& content);
  google_apis::GDataErrorCode RemoveResource(
      const std::string& file_id);
  google_apis::GDataErrorCode GetSyncRootFolderID(
      std::string* sync_root_folder_id);
  google_apis::GDataErrorCode ListFilesInFolder(
      const std::string& folder_id,
      ScopedVector<google_apis::ResourceEntry>* entries);
  google_apis::GDataErrorCode SearchByTitle(
      const std::string& folder_id,
      const std::string& title,
      ScopedVector<google_apis::ResourceEntry>* entries);
  google_apis::GDataErrorCode GetResourceEntry(
      const std::string& file_id,
      scoped_ptr<google_apis::ResourceEntry>* entry);
  google_apis::GDataErrorCode ReadFile(
      const std::string& file_id,
      std::string* file_content);

  base::FilePath base_dir_path() { return base_dir_.path(); }

 private:
  google_apis::GDataErrorCode CompleteListing(
      scoped_ptr<google_apis::ResourceList> list,
      ScopedVector<google_apis::ResourceEntry> * entries);

  void Initialize();

  base::FilePath WriteToTempFile(const std::string& content);
  void FlushMessageLoop();

  base::ScopedTempDir base_dir_;
  base::FilePath temp_dir_;

  // Not own.
  drive::FakeDriveService* fake_drive_service_;
  drive::DriveUploaderInterface* drive_uploader_;
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_FAKE_DRIVE_SERVICE_HELPER_H_
