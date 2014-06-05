// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FAKE_DRIVE_UPLOADER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FAKE_DRIVE_UPLOADER_H_

#include <string>

#include "base/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_service_helper.h"
#include "google_apis/drive/gdata_errorcode.h"
#include "google_apis/drive/test_util.h"
#include "net/base/escape.h"

namespace sync_file_system {
namespace drive_backend {

class FakeDriveServiceWrapper : public drive::FakeDriveService {
 public:
  FakeDriveServiceWrapper();
  virtual ~FakeDriveServiceWrapper();

  // DriveServiceInterface overrides.
  virtual google_apis::CancelCallback AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_name,
      const AddNewDirectoryOptions& options,
      const google_apis::FileResourceCallback& callback) OVERRIDE;

  void set_make_directory_conflict(bool enable) {
    make_directory_conflict_ = enable;
  }

 private:
  bool make_directory_conflict_;

  DISALLOW_COPY_AND_ASSIGN(FakeDriveServiceWrapper);
};

// A fake implementation of DriveUploaderInterface, which provides fake
// behaviors for file uploading.
class FakeDriveUploader : public drive::DriveUploaderInterface {
 public:
  explicit FakeDriveUploader(FakeDriveServiceWrapper* fake_drive_service);
  virtual ~FakeDriveUploader();

  // DriveUploaderInterface overrides.
  virtual google_apis::CancelCallback UploadNewFile(
      const std::string& parent_resource_id,
      const base::FilePath& local_file_path,
      const std::string& title,
      const std::string& content_type,
      const UploadNewFileOptions& options,
      const drive::UploadCompletionCallback& callback,
      const google_apis::ProgressCallback& progress_callback) OVERRIDE;
  virtual google_apis::CancelCallback UploadExistingFile(
      const std::string& resource_id,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      const UploadExistingFileOptions& options,
      const drive::UploadCompletionCallback& callback,
      const google_apis::ProgressCallback& progress_callback) OVERRIDE;
  virtual google_apis::CancelCallback ResumeUploadFile(
      const GURL& upload_location,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      const drive::UploadCompletionCallback& callback,
      const google_apis::ProgressCallback& progress_callback) OVERRIDE;

  void set_make_file_conflict(bool enable) {
    make_file_conflict_ = enable;
  }

 private:
  FakeDriveServiceWrapper* fake_drive_service_;
  bool make_file_conflict_;

  DISALLOW_COPY_AND_ASSIGN(FakeDriveUploader);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FAKE_DRIVE_UPLOADER_H_
