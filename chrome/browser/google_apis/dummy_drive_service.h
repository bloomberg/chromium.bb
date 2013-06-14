// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DUMMY_DRIVE_SERVICE_H_
#define CHROME_BROWSER_GOOGLE_APIS_DUMMY_DRIVE_SERVICE_H_

#include "chrome/browser/google_apis/drive_service_interface.h"

namespace google_apis {

// Dummy implementation of DriveServiceInterface.
// All functions do nothing, or return place holder values like 'true'.
class DummyDriveService : public DriveServiceInterface {
 public:
  DummyDriveService();
  virtual ~DummyDriveService();

  // DriveServiceInterface Overrides
  virtual void Initialize(Profile* profile) OVERRIDE;
  virtual void AddObserver(DriveServiceObserver* observer) OVERRIDE;
  virtual void RemoveObserver(DriveServiceObserver* observer) OVERRIDE;
  virtual bool CanSendRequest() const OVERRIDE;
  virtual void CancelAll() OVERRIDE;
  virtual bool CancelForFilePath(const base::FilePath& file_path) OVERRIDE;
  virtual std::string CanonicalizeResourceId(
      const std::string& resource_id) const OVERRIDE;
  virtual bool HasAccessToken() const OVERRIDE;
  virtual bool HasRefreshToken() const OVERRIDE;
  virtual void ClearAccessToken() OVERRIDE;
  virtual void ClearRefreshToken() OVERRIDE;
  virtual std::string GetRootResourceId() const OVERRIDE;
  virtual CancelCallback GetAllResourceList(
      const GetResourceListCallback& callback) OVERRIDE;
  virtual CancelCallback GetResourceListInDirectory(
      const std::string& directory_resource_id,
      const GetResourceListCallback& callback) OVERRIDE;
  virtual CancelCallback Search(
      const std::string& search_query,
      const GetResourceListCallback& callback) OVERRIDE;
  virtual CancelCallback SearchByTitle(
      const std::string& title,
      const std::string& directory_resource_id,
      const GetResourceListCallback& callback) OVERRIDE;
  virtual CancelCallback GetChangeList(
      int64 start_changestamp,
      const GetResourceListCallback& callback) OVERRIDE;
  virtual CancelCallback ContinueGetResourceList(
      const GURL& override_url,
      const GetResourceListCallback& callback) OVERRIDE;
  virtual CancelCallback GetResourceEntry(
      const std::string& resource_id,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual CancelCallback GetAboutResource(
      const GetAboutResourceCallback& callback) OVERRIDE;
  virtual CancelCallback GetAppList(
      const GetAppListCallback& callback) OVERRIDE;
  virtual CancelCallback DeleteResource(
      const std::string& resource_id,
      const std::string& etag,
      const EntryActionCallback& callback) OVERRIDE;
  virtual CancelCallback DownloadFile(
      const base::FilePath& virtual_path,
      const base::FilePath& local_cache_path,
      const GURL& download_url,
      const DownloadActionCallback& download_action_callback,
      const GetContentCallback& get_content_callback,
      const ProgressCallback& progress_callback) OVERRIDE;
  virtual CancelCallback CopyResource(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_name,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual CancelCallback CopyHostedDocument(
      const std::string& resource_id,
      const std::string& new_name,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual CancelCallback RenameResource(
      const std::string& resource_id,
      const std::string& new_name,
      const EntryActionCallback& callback) OVERRIDE;
  virtual CancelCallback TouchResource(
      const std::string& resource_id,
      const base::Time& modified_date,
      const base::Time& last_viewed_by_me_date,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual CancelCallback AddResourceToDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const EntryActionCallback& callback) OVERRIDE;
  virtual CancelCallback RemoveResourceFromDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const EntryActionCallback& callback) OVERRIDE;
  virtual CancelCallback AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_name,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual CancelCallback InitiateUploadNewFile(
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const InitiateUploadCallback& callback) OVERRIDE;
  virtual CancelCallback InitiateUploadExistingFile(
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& resource_id,
      const std::string& etag,
      const InitiateUploadCallback& callback) OVERRIDE;
  virtual CancelCallback ResumeUpload(
      const base::FilePath& drive_file_path,
      const GURL& upload_url,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      const UploadRangeCallback& callback,
      const ProgressCallback& progress_callback) OVERRIDE;
  virtual CancelCallback GetUploadStatus(
      const base::FilePath& drive_file_path,
      const GURL& upload_url,
      int64 content_length,
      const UploadRangeCallback& callback) OVERRIDE;
  virtual CancelCallback AuthorizeApp(
      const std::string& resource_id,
      const std::string& app_id,
      const AuthorizeAppCallback& callback) OVERRIDE;
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DUMMY_DRIVE_SERVICE_H_
