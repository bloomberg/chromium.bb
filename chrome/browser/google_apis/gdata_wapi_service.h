// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_SERVICE_H_
#define CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_SERVICE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/google_apis/auth_service_observer.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/google_apis/gdata_operations.h"

class FilePath;
class GURL;
class Profile;

namespace gdata {
class AuthService;
class OperationRunner;
}

namespace drive {

// This class provides documents feed service calls for WAPI (codename for
// DocumentsList API).
// Details of API call are abstracted in each operation class and this class
// works as a thin wrapper for the API.
class GDataWapiService : public DriveServiceInterface,
                         public gdata::AuthServiceObserver,
                         public gdata::OperationRegistryObserver {
 public:
  // Instance is usually created by DriveSystemServiceFactory and owned by
  // DriveFileSystem.
  GDataWapiService();
  virtual ~GDataWapiService();

  gdata::AuthService* auth_service_for_testing();

  // DriveServiceInterface Overrides
  virtual void Initialize(Profile* profile) OVERRIDE;
  virtual void AddObserver(DriveServiceObserver* observer) OVERRIDE;
  virtual void RemoveObserver(DriveServiceObserver* observer) OVERRIDE;
  virtual bool CanStartOperation() const OVERRIDE;
  virtual void CancelAll() OVERRIDE;
  virtual bool CancelForFilePath(const FilePath& file_path) OVERRIDE;
  virtual gdata::OperationProgressStatusList GetProgressStatusList()
      const OVERRIDE;
  virtual void Authenticate(
      const gdata::AuthStatusCallback& callback) OVERRIDE;
  virtual bool HasAccessToken() const OVERRIDE;
  virtual bool HasRefreshToken() const OVERRIDE;
  virtual void GetDocuments(const GURL& feed_url,
                            int64 start_changestamp,
                            const std::string& search_query,
                            const std::string& directory_resource_id,
                            const gdata::GetDataCallback& callback) OVERRIDE;
  virtual void GetDocumentEntry
  (const std::string& resource_id,
   const gdata::GetDataCallback& callback) OVERRIDE;

  virtual void GetAccountMetadata(
      const gdata::GetDataCallback& callback) OVERRIDE;
  virtual void GetApplicationInfo(
      const gdata::GetDataCallback& callback) OVERRIDE;
  virtual void DeleteDocument(
      const GURL& document_url,
      const gdata::EntryActionCallback& callback) OVERRIDE;
  virtual void DownloadDocument(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      DocumentExportFormat format,
      const gdata::DownloadActionCallback& callback) OVERRIDE;
  virtual void DownloadFile(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      const gdata::DownloadActionCallback& download_action_callback,
      const gdata::GetContentCallback& get_content_callback) OVERRIDE;
  virtual void CopyDocument(const std::string& resource_id,
                            const FilePath::StringType& new_name,
                            const gdata::GetDataCallback& callback) OVERRIDE;
  virtual void RenameResource(
      const GURL& document_url,
      const FilePath::StringType& new_name,
      const gdata::EntryActionCallback& callback) OVERRIDE;
  virtual void AddResourceToDirectory(
      const GURL& parent_content_url,
      const GURL& resource_url,
      const gdata::EntryActionCallback& callback) OVERRIDE;
  virtual void RemoveResourceFromDirectory(
      const GURL& parent_content_url,
      const GURL& resource_url,
      const std::string& resource_id,
      const gdata::EntryActionCallback& callback) OVERRIDE;
  virtual void CreateDirectory(
      const GURL& parent_content_url,
      const FilePath::StringType& directory_name,
      const gdata::GetDataCallback& callback) OVERRIDE;
  virtual void InitiateUpload(
      const gdata::InitiateUploadParams& params,
      const gdata::InitiateUploadCallback& callback) OVERRIDE;
  virtual void ResumeUpload(
      const gdata::ResumeUploadParams& params,
      const gdata::ResumeUploadCallback& callback) OVERRIDE;
  virtual void AuthorizeApp(const GURL& resource_url,
                            const std::string& app_id,
                            const gdata::GetDataCallback& callback) OVERRIDE;

 private:
  gdata::OperationRegistry* operation_registry() const;

  // AuthService::Observer override.
  virtual void OnOAuth2RefreshTokenChanged() OVERRIDE;

  // DriveServiceObserver Overrides
  virtual void OnProgressUpdate(
      const gdata::OperationProgressStatusList& list) OVERRIDE;
  virtual void OnAuthenticationFailed(gdata::GDataErrorCode error) OVERRIDE;

  scoped_ptr<gdata::OperationRunner> runner_;
  ObserverList<DriveServiceObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(GDataWapiService);
};

}  // namespace drive

#endif  // CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_SERVICE_H_
