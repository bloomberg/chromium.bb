// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_SERVICE_ON_WORKER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_SERVICE_ON_WORKER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/drive/drive_service_interface.h"

namespace base {
class SingleThreadTaskRunner;
class SequencedTaskRunner;
}

namespace sync_file_system {
namespace drive_backend {

class DriveServiceWrapper;

// This class wraps a part of DriveServiceInterface class to post actual
// tasks to DriveServiceWrapper which lives in another thread.
// Each method wraps corresponding name method of DriveServiceInterface.
// See comments in drive_service_interface.h for details.
class DriveServiceOnWorker : public drive::DriveServiceInterface {
 public:
  DriveServiceOnWorker(
      const base::WeakPtr<DriveServiceWrapper>& wrapper,
      base::SingleThreadTaskRunner* ui_task_runner,
      base::SequencedTaskRunner* worker_task_runner);
  virtual ~DriveServiceOnWorker();

  virtual google_apis::CancelCallback AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_title,
      const AddNewDirectoryOptions& options,
      const google_apis::FileResourceCallback& callback) OVERRIDE;

  virtual google_apis::CancelCallback DeleteResource(
      const std::string& resource_id,
      const std::string& etag,
      const google_apis::EntryActionCallback& callback) OVERRIDE;

  virtual google_apis::CancelCallback DownloadFile(
      const base::FilePath& local_cache_path,
      const std::string& resource_id,
      const google_apis::DownloadActionCallback& download_action_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const google_apis::ProgressCallback& progress_callback) OVERRIDE;

  virtual google_apis::CancelCallback GetAboutResource(
      const google_apis::AboutResourceCallback& callback) OVERRIDE;

  virtual google_apis::CancelCallback GetChangeList(
      int64 start_changestamp,
      const google_apis::ChangeListCallback& callback) OVERRIDE;

  virtual google_apis::CancelCallback GetRemainingChangeList(
      const GURL& next_link,
      const google_apis::ChangeListCallback& callback) OVERRIDE;

  virtual std::string GetRootResourceId() const OVERRIDE;

  virtual google_apis::CancelCallback GetRemainingFileList(
      const GURL& next_link,
      const google_apis::FileListCallback& callback) OVERRIDE;

  virtual google_apis::CancelCallback GetFileResource(
      const std::string& resource_id,
      const google_apis::FileResourceCallback& callback) OVERRIDE;

  virtual google_apis::CancelCallback GetFileListInDirectory(
      const std::string& directory_resource_id,
      const google_apis::FileListCallback& callback) OVERRIDE;

  virtual google_apis::CancelCallback RemoveResourceFromDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback) OVERRIDE;

  virtual google_apis::CancelCallback SearchByTitle(
      const std::string& title,
      const std::string& directory_resource_id,
      const google_apis::FileListCallback& callback) OVERRIDE;

  virtual bool HasRefreshToken() const OVERRIDE;

  // Following virtual methods are expected not to be accessed at all.
  virtual void Initialize(const std::string& account_id) OVERRIDE;
  virtual void AddObserver(drive::DriveServiceObserver* observer) OVERRIDE;
  virtual void RemoveObserver(drive::DriveServiceObserver* observer) OVERRIDE;
  virtual bool CanSendRequest() const OVERRIDE;
  virtual bool HasAccessToken() const OVERRIDE;
  virtual void RequestAccessToken(
      const google_apis::AuthStatusCallback& callback) OVERRIDE;
  virtual void ClearAccessToken() OVERRIDE;
  virtual void ClearRefreshToken() OVERRIDE;
  virtual google_apis::CancelCallback GetAllFileList(
      const google_apis::FileListCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback Search(
      const std::string& search_query,
      const google_apis::FileListCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback GetShareUrl(
      const std::string& resource_id,
      const GURL& embed_origin,
      const google_apis::GetShareUrlCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback GetAppList(
      const google_apis::AppListCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback TrashResource(
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback CopyResource(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_title,
      const base::Time& last_modified,
      const google_apis::FileResourceCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback UpdateResource(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_title,
      const base::Time& last_modified,
      const base::Time& last_viewed_by_me,
      const google_apis::FileResourceCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback AddResourceToDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback InitiateUploadNewFile(
      const std::string& content_type,
      int64 content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const InitiateUploadNewFileOptions& options,
      const google_apis::InitiateUploadCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback InitiateUploadExistingFile(
      const std::string& content_type,
      int64 content_length,
      const std::string& resource_id,
      const InitiateUploadExistingFileOptions& options,
      const google_apis::InitiateUploadCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback ResumeUpload(
      const GURL& upload_url,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      const google_apis::drive::UploadRangeCallback& callback,
      const google_apis::ProgressCallback& progress_callback) OVERRIDE;
  virtual google_apis::CancelCallback GetUploadStatus(
      const GURL& upload_url,
      int64 content_length,
      const google_apis::drive::UploadRangeCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback AuthorizeApp(
      const std::string& resource_id,
      const std::string& app_id,
      const google_apis::AuthorizeAppCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback UninstallApp(
      const std::string& app_id,
      const google_apis::EntryActionCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback AddPermission(
      const std::string& resource_id,
      const std::string& email,
      google_apis::drive::PermissionRole role,
      const google_apis::EntryActionCallback& callback) OVERRIDE;

 private:
  base::WeakPtr<DriveServiceWrapper> wrapper_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(DriveServiceOnWorker);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_SERVICE_ON_WORKER_H_
