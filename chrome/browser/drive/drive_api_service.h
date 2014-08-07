// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DRIVE_DRIVE_API_SERVICE_H_
#define CHROME_BROWSER_DRIVE_DRIVE_API_SERVICE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "google_apis/drive/auth_service_interface.h"
#include "google_apis/drive/auth_service_observer.h"
#include "google_apis/drive/drive_api_url_generator.h"
#include "google_apis/drive/gdata_wapi_url_generator.h"

class GURL;
class OAuth2TokenService;

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace google_apis {
class RequestSender;
}

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace drive {

// This class provides Drive request calls using Drive V2 API.
// Details of API call are abstracted in each request class and this class
// works as a thin wrapper for the API.
class DriveAPIService : public DriveServiceInterface,
                        public google_apis::AuthServiceObserver {
 public:
  // |oauth2_token_service| is used for obtaining OAuth2 access tokens.
  // |url_request_context_getter| is used to initialize URLFetcher.
  // |blocking_task_runner| is used to run blocking tasks (like parsing JSON).
  // |base_url| is used to generate URLs for communication with the drive API.
  // |base_download_url| is used to generate URLs for downloading file from the
  // drive API.
  // |wapi_base_url| is used to generate URLs for shared_url. Unfortunately,
  // the share_url is not yet supported on Drive API v2, so as a fallback,
  // we use GData WAPI.
  // |custom_user_agent| will be used for the User-Agent header in HTTP
  // requests issues through the service if the value is not empty.
  DriveAPIService(
      OAuth2TokenService* oauth2_token_service,
      net::URLRequestContextGetter* url_request_context_getter,
      base::SequencedTaskRunner* blocking_task_runner,
      const GURL& base_url,
      const GURL& base_download_url,
      const GURL& wapi_base_url,
      const std::string& custom_user_agent);
  virtual ~DriveAPIService();

  // DriveServiceInterface Overrides
  virtual void Initialize(const std::string& account_id) OVERRIDE;
  virtual void AddObserver(DriveServiceObserver* observer) OVERRIDE;
  virtual void RemoveObserver(DriveServiceObserver* observer) OVERRIDE;
  virtual bool CanSendRequest() const OVERRIDE;
  virtual bool HasAccessToken() const OVERRIDE;
  virtual void RequestAccessToken(
      const google_apis::AuthStatusCallback& callback) OVERRIDE;
  virtual bool HasRefreshToken() const OVERRIDE;
  virtual void ClearAccessToken() OVERRIDE;
  virtual void ClearRefreshToken() OVERRIDE;
  virtual std::string GetRootResourceId() const OVERRIDE;
  virtual google_apis::CancelCallback GetAllFileList(
      const google_apis::FileListCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback GetFileListInDirectory(
      const std::string& directory_resource_id,
      const google_apis::FileListCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback Search(
      const std::string& search_query,
      const google_apis::FileListCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback SearchByTitle(
      const std::string& title,
      const std::string& directory_resource_id,
      const google_apis::FileListCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback GetChangeList(
      int64 start_changestamp,
      const google_apis::ChangeListCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback GetRemainingChangeList(
      const GURL& next_link,
      const google_apis::ChangeListCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback GetRemainingFileList(
      const GURL& next_link,
      const google_apis::FileListCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback GetFileResource(
      const std::string& resource_id,
      const google_apis::FileResourceCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback GetShareUrl(
      const std::string& resource_id,
      const GURL& embed_origin,
      const google_apis::GetShareUrlCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback GetAboutResource(
      const google_apis::AboutResourceCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback GetAppList(
      const google_apis::AppListCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback DeleteResource(
      const std::string& resource_id,
      const std::string& etag,
      const google_apis::EntryActionCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback TrashResource(
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback DownloadFile(
      const base::FilePath& local_cache_path,
      const std::string& resource_id,
      const google_apis::DownloadActionCallback& download_action_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const google_apis::ProgressCallback& progress_callback) OVERRIDE;
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
  virtual google_apis::CancelCallback RemoveResourceFromDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback) OVERRIDE;
  virtual google_apis::CancelCallback AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_title,
      const AddNewDirectoryOptions& options,
      const google_apis::FileResourceCallback& callback) OVERRIDE;
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
  // AuthServiceObserver override.
  virtual void OnOAuth2RefreshTokenChanged() OVERRIDE;

  OAuth2TokenService* oauth2_token_service_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<google_apis::RequestSender> sender_;
  ObserverList<DriveServiceObserver> observers_;
  google_apis::DriveApiUrlGenerator url_generator_;
  google_apis::GDataWapiUrlGenerator wapi_url_generator_;
  const std::string custom_user_agent_;

  DISALLOW_COPY_AND_ASSIGN(DriveAPIService);
};

}  // namespace drive

#endif  // CHROME_BROWSER_DRIVE_DRIVE_API_SERVICE_H_
