// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_SERVICE_WRAPPER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_SERVICE_WRAPPER_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/drive/drive_service_interface.h"

namespace sync_file_system {
namespace drive_backend {

// This class wraps a part of DriveServiceInterface class to support weak
// pointer.  Each method wraps corresponding name method of
// DriveServiceInterface.  See comments in drive_service_interface.h
// for details.
class DriveServiceWrapper : public base::SupportsWeakPtr<DriveServiceWrapper> {
 public:
  explicit DriveServiceWrapper(drive::DriveServiceInterface* drive_service);

  void AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_title,
      const drive::DriveServiceInterface::AddNewDirectoryOptions& options,
      const google_apis::FileResourceCallback& callback);

  void DeleteResource(
      const std::string& resource_id,
      const std::string& etag,
      const google_apis::EntryActionCallback& callback);

  void DownloadFile(
      const base::FilePath& local_cache_path,
      const std::string& resource_id,
      const google_apis::DownloadActionCallback& download_action_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const google_apis::ProgressCallback& progress_callback);

  void GetAboutResource(
      const google_apis::AboutResourceCallback& callback);

  void GetChangeList(
      int64 start_changestamp,
      const google_apis::ChangeListCallback& callback);

  void GetRemainingChangeList(
      const GURL& next_link,
      const google_apis::ChangeListCallback& callback);

  void GetRemainingFileList(
      const GURL& next_link,
      const google_apis::FileListCallback& callback);

  void GetFileResource(
      const std::string& resource_id,
      const google_apis::FileResourceCallback& callback);

  void GetFileListInDirectory(
      const std::string& directory_resource_id,
      const google_apis::FileListCallback& callback);

  void RemoveResourceFromDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback);

  void SearchByTitle(
      const std::string& title,
      const std::string& directory_resource_id,
      const google_apis::FileListCallback& callback);

 private:
  drive::DriveServiceInterface* drive_service_;
  base::SequenceChecker sequece_checker_;

  DISALLOW_COPY_AND_ASSIGN(DriveServiceWrapper);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_SERVICE_WRAPPER_H_
