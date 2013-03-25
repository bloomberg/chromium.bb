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
  virtual bool CanStartOperation() const OVERRIDE;
  virtual void CancelAll() OVERRIDE;
  virtual bool CancelForFilePath(const base::FilePath& file_path) OVERRIDE;
  virtual OperationProgressStatusList GetProgressStatusList() const OVERRIDE;
  virtual bool HasAccessToken() const OVERRIDE;
  virtual bool HasRefreshToken() const OVERRIDE;
  virtual void ClearAccessToken() OVERRIDE;
  virtual void ClearRefreshToken() OVERRIDE;
  virtual std::string GetRootResourceId() const OVERRIDE;
  virtual void GetResourceList(
      const GURL& url,
      int64 start_changestamp,
      const std::string& search_query,
      bool shared_with_me,
      const std::string& directory_resource_id,
      const GetResourceListCallback& callback) OVERRIDE;
  virtual void GetResourceEntry(
      const std::string& resource_id,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual void GetAccountMetadata(
      const GetAccountMetadataCallback& callback) OVERRIDE;
  virtual void GetAboutResource(
      const GetAboutResourceCallback& callback) OVERRIDE;
  virtual void GetAppList(const GetAppListCallback& callback) OVERRIDE;
  virtual void DeleteResource(const std::string& resource_id,
                              const std::string& etag,
                              const EntryActionCallback& callback) OVERRIDE;
  virtual void DownloadFile(
      const base::FilePath& virtual_path,
      const base::FilePath& local_cache_path,
      const GURL& download_url,
      const DownloadActionCallback& download_action_callback,
      const GetContentCallback& get_content_callback) OVERRIDE;
  virtual void CopyHostedDocument(
      const std::string& resource_id,
      const std::string& new_name,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual void RenameResource(const std::string& resource_id,
                              const std::string& new_name,
                              const EntryActionCallback& callback) OVERRIDE;
  virtual void AddResourceToDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const EntryActionCallback& callback) OVERRIDE;
  virtual void RemoveResourceFromDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const EntryActionCallback& callback) OVERRIDE;
  virtual void AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_name,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual void InitiateUploadNewFile(
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const InitiateUploadCallback& callback) OVERRIDE;
  virtual void InitiateUploadExistingFile(
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& resource_id,
      const std::string& etag,
      const InitiateUploadCallback& callback) OVERRIDE;
  virtual void ResumeUpload(
      UploadMode upload_mode,
      const base::FilePath& drive_file_path,
      const GURL& upload_url,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const scoped_refptr<net::IOBuffer>& buf,
      const UploadRangeCallback& callback) OVERRIDE;
  virtual void GetUploadStatus(
      UploadMode upload_mode,
      const base::FilePath& drive_file_path,
      const GURL& upload_url,
      int64 content_length,
      const UploadRangeCallback& callback) OVERRIDE;
  virtual void AuthorizeApp(const std::string& resource_id,
                            const std::string& app_id,
                            const AuthorizeAppCallback& callback) OVERRIDE;
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DUMMY_DRIVE_SERVICE_H_
