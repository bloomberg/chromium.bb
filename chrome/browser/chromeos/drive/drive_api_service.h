// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_API_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_API_SERVICE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/auth_service_observer.h"
#include "chrome/browser/google_apis/drive_api_url_generator.h"
#include "chrome/browser/google_apis/drive_service_interface.h"

class FilePath;
class GURL;
class Profile;

namespace google_apis {
class OperationRunner;
}  // namespace google_apis

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace drive {

// This class provides documents feed service calls for Drive V2 API.
// Details of API call are abstracted in each operation class and this class
// works as a thin wrapper for the API.
class DriveAPIService : public google_apis::DriveServiceInterface,
                        public google_apis::AuthServiceObserver,
                        public google_apis::OperationRegistryObserver {
 public:
  // Instance is usually created by DriveSystemServiceFactory and owned by
  // DriveFileSystem.
  //
  // |url_request_context_getter| is used to initialize URLFetcher.
  // |custom_user_agent| will be used for the User-Agent header in HTTP
  // requests issues through the service if the value is not empty.
  DriveAPIService(
      net::URLRequestContextGetter* url_request_context_getter,
      const std::string& custom_user_agent);
  virtual ~DriveAPIService();

  // DriveServiceInterface Overrides
  virtual void Initialize(Profile* profile) OVERRIDE;
  virtual void AddObserver(
      google_apis::DriveServiceObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      google_apis::DriveServiceObserver* observer) OVERRIDE;
  virtual bool CanStartOperation() const OVERRIDE;
  virtual void CancelAll() OVERRIDE;
  virtual bool CancelForFilePath(const FilePath& file_path) OVERRIDE;
  virtual google_apis::OperationProgressStatusList GetProgressStatusList()
      const OVERRIDE;
  virtual bool HasAccessToken() const OVERRIDE;
  virtual bool HasRefreshToken() const OVERRIDE;
  virtual void GetResourceList(
      const GURL& feed_url,
      int64 start_changestamp,
      const std::string& search_query,
      bool shared_with_me,
      const std::string& directory_resource_id,
      const google_apis::GetResourceListCallback& callback) OVERRIDE;
  virtual void GetResourceEntry(
      const std::string& resource_id,
      const google_apis::GetResourceEntryCallback& callback) OVERRIDE;

  virtual void GetAccountMetadata(
      const google_apis::GetAccountMetadataCallback& callback) OVERRIDE;
  virtual void GetApplicationInfo(
      const google_apis::GetDataCallback& callback) OVERRIDE;
  virtual void DeleteResource(
      const GURL& edit_url,
      const google_apis::EntryActionCallback& callback) OVERRIDE;
  virtual void DownloadHostedDocument(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      google_apis::DocumentExportFormat format,
      const google_apis::DownloadActionCallback& callback) OVERRIDE;
  virtual void DownloadFile(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      const google_apis::DownloadActionCallback& download_action_callback,
      const google_apis::GetContentCallback& get_content_callback) OVERRIDE;
  virtual void CopyHostedDocument(
      const std::string& resource_id,
      const FilePath::StringType& new_name,
      const google_apis::GetResourceEntryCallback& callback) OVERRIDE;
  virtual void RenameResource(
      const GURL& edit_url,
      const FilePath::StringType& new_name,
      const google_apis::EntryActionCallback& callback) OVERRIDE;
  virtual void AddResourceToDirectory(
      const GURL& parent_content_url,
      const GURL& edit_url,
      const google_apis::EntryActionCallback& callback) OVERRIDE;
  virtual void RemoveResourceFromDirectory(
      const GURL& parent_content_url,
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback) OVERRIDE;
  virtual void AddNewDirectory(
      const GURL& parent_content_url,
      const FilePath::StringType& directory_name,
      const google_apis::GetResourceEntryCallback& callback) OVERRIDE;
  virtual void InitiateUpload(
      const google_apis::InitiateUploadParams& params,
      const google_apis::InitiateUploadCallback& callback) OVERRIDE;
  virtual void ResumeUpload(
      const google_apis::ResumeUploadParams& params,
      const google_apis::ResumeUploadCallback& callback) OVERRIDE;
  virtual void AuthorizeApp(
      const GURL& edit_url,
      const std::string& app_id,
      const google_apis::AuthorizeAppCallback& callback) OVERRIDE;

 private:
  google_apis::OperationRegistry* operation_registry() const;

  // Fetches a changelist from |url| with |start_changestamp|, using Drive V2
  // API. If this URL is empty the call will use the default URL. Specify |url|
  // when pagenated request should be issued.
  // |start_changestamp| specifies the starting point of change list or 0 if
  // all changes are necessary.
  // Upon completion, invokes |callback| with results on calling thread.
  void GetChangelist(const GURL& url,
                     int64 start_changestamp,
                     const google_apis::GetResourceListCallback& callback);

  // Fetches a filelist from |url| with |search_query|, using Drive V2 API. If
  // this URL is empty the call will use the default URL. Specify |url| when
  // pagenated request should be issued.
  // |search_query| specifies query string, whose syntax is described at
  // https://developers.google.com/drive/search-parameters
  void GetFilelist(const GURL& url,
                   const std::string& search_query,
                   const google_apis::GetResourceListCallback& callback);

  // AuthService::Observer override.
  virtual void OnOAuth2RefreshTokenChanged() OVERRIDE;

  // DriveServiceObserver Overrides
  virtual void OnProgressUpdate(
      const google_apis::OperationProgressStatusList& list) OVERRIDE;
  virtual void OnAuthenticationFailed(
      google_apis::GDataErrorCode error) OVERRIDE;

  net::URLRequestContextGetter* url_request_context_getter_;
  Profile* profile_;
  scoped_ptr<google_apis::OperationRunner> runner_;
  ObserverList<google_apis::DriveServiceObserver> observers_;
  google_apis::DriveApiUrlGenerator url_generator_;
  const std::string custom_user_agent_;

  DISALLOW_COPY_AND_ASSIGN(DriveAPIService);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_API_SERVICE_H_
