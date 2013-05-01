// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_SERVICE_H_
#define CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_SERVICE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/google_apis/auth_service_observer.h"
#include "chrome/browser/google_apis/drive_api_url_generator.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"

class GURL;
class Profile;

namespace base {
class FilePath;
}

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace google_apis {
class OperationRunner;

// This class provides Drive operation calls using Drive V2 API.
// Details of API call are abstracted in each operation class and this class
// works as a thin wrapper for the API.
class DriveAPIService : public DriveServiceInterface,
                        public AuthServiceObserver {
 public:
  // |url_request_context_getter| is used to initialize URLFetcher.
  // |base_url| is used to generate URLs for communication with the drive API.
  // |wapi_base_url| is used to generate URLs for communication with
  // the GData WAPI server. Note that this should only be used for the hacky
  // workaround for the operations which is not-yet supported on Drive API v2.
  // |custom_user_agent| will be used for the User-Agent header in HTTP
  // requests issues through the service if the value is not empty.
  DriveAPIService(
      net::URLRequestContextGetter* url_request_context_getter,
      const GURL& base_url,
      const GURL& wapi_base_url,
      const std::string& custom_user_agent);
  virtual ~DriveAPIService();

  // DriveServiceInterface Overrides
  virtual void Initialize(Profile* profile) OVERRIDE;
  virtual void AddObserver(DriveServiceObserver* observer) OVERRIDE;
  virtual void RemoveObserver(DriveServiceObserver* observer) OVERRIDE;
  virtual bool CanStartOperation() const OVERRIDE;
  virtual void CancelAll() OVERRIDE;
  virtual bool CancelForFilePath(const base::FilePath& file_path) OVERRIDE;
  virtual bool HasAccessToken() const OVERRIDE;
  virtual bool HasRefreshToken() const OVERRIDE;
  virtual void ClearAccessToken() OVERRIDE;
  virtual void ClearRefreshToken() OVERRIDE;
  virtual std::string GetRootResourceId() const OVERRIDE;
  virtual void GetAllResourceList(
      const GetResourceListCallback& callback) OVERRIDE;
  virtual void GetResourceListInDirectory(
      const std::string& directory_resource_id,
      const GetResourceListCallback& callback) OVERRIDE;
  virtual void Search(
      const std::string& search_query,
      const GetResourceListCallback& callback) OVERRIDE;
  virtual void SearchByTitle(
      const std::string& title,
      const std::string& directory_resource_id,
      const GetResourceListCallback& callback) OVERRIDE;
  virtual void GetChangeList(
      int64 start_changestamp,
      const GetResourceListCallback& callback) OVERRIDE;
  virtual void ContinueGetResourceList(
      const GURL& override_url,
      const GetResourceListCallback& callback) OVERRIDE;
  virtual void GetResourceEntry(
      const std::string& resource_id,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual void GetAccountMetadata(
      const GetAccountMetadataCallback& callback) OVERRIDE;
  virtual void GetAboutResource(
      const GetAboutResourceCallback& callback) OVERRIDE;
  virtual void GetAppList(const GetAppListCallback& callback) OVERRIDE;
  virtual void DeleteResource(
      const std::string& resource_id,
      const std::string& etag,
      const EntryActionCallback& callback) OVERRIDE;
  virtual void DownloadFile(
      const base::FilePath& virtual_path,
      const base::FilePath& local_cache_path,
      const GURL& download_url,
      const DownloadActionCallback& download_action_callback,
      const GetContentCallback& get_content_callback,
      const ProgressCallback& progress_callback) OVERRIDE;
  virtual void CopyHostedDocument(
      const std::string& resource_id,
      const std::string& new_name,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual void RenameResource(
      const std::string& resource_id,
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
      const UploadRangeCallback& callback,
      const ProgressCallback& progress_callback) OVERRIDE;
  virtual void GetUploadStatus(
      UploadMode upload_mode,
      const base::FilePath& drive_file_path,
      const GURL& upload_url,
      int64 content_length,
      const UploadRangeCallback& callback) OVERRIDE;
  virtual void AuthorizeApp(
      const std::string& resource_id,
      const std::string& app_id,
      const AuthorizeAppCallback& callback) OVERRIDE;

 private:
  OperationRegistry* operation_registry() const;

  // AuthServiceObserver override.
  virtual void OnOAuth2RefreshTokenChanged() OVERRIDE;

  net::URLRequestContextGetter* url_request_context_getter_;
  Profile* profile_;
  scoped_ptr<OperationRunner> runner_;
  ObserverList<DriveServiceObserver> observers_;
  DriveApiUrlGenerator url_generator_;
  GDataWapiUrlGenerator wapi_url_generator_;
  const std::string custom_user_agent_;

  DISALLOW_COPY_AND_ASSIGN(DriveAPIService);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_SERVICE_H_
