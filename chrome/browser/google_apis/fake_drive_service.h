// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_FAKE_DRIVE_SERVICE_H_
#define CHROME_BROWSER_GOOGLE_APIS_FAKE_DRIVE_SERVICE_H_

#include "base/values.h"
#include "chrome/browser/google_apis/drive_service_interface.h"

namespace google_apis {

// This class implements a fake DriveService which acts like a real Drive
// service. The fake service works as follows:
//
// 1) Load JSON files and construct the in-memory resource list.
// 2) Return valid responses based on the the in-memory resource list.
// 3) Update the in-memory resource list by operations like DeleteResource().
//
// TODO(satorux): Implement the advertised behaviors. crbug.com/162350
class FakeDriveService : public DriveServiceInterface {
 public:
  FakeDriveService();
  virtual ~FakeDriveService();

  // Loads the resource list for WAPI. Returns true on success.
  bool LoadResourceListForWapi(const std::string& relative_path);

  // Loads the account metadata for WAPI. Returns true on success.
  bool LoadAccountMetadataForWapi(const std::string& relative_path);

  // Loads the application info for Drive API. Returns true on success.
  bool LoadApplicationInfoForDriveApi(const std::string& relative_path);

  // Changes the offline state. All functions fail with GDATA_NO_CONNECTION
  // when offline. By default the offline state is false.
  void set_offline(bool offline) { offline_ = offline; }

  // DriveServiceInterface Overrides
  virtual void Initialize(Profile* profile) OVERRIDE;
  virtual void AddObserver(DriveServiceObserver* observer) OVERRIDE;
  virtual void RemoveObserver(DriveServiceObserver* observer) OVERRIDE;
  virtual bool CanStartOperation() const OVERRIDE;
  virtual void CancelAll() OVERRIDE;
  virtual bool CancelForFilePath(const FilePath& file_path) OVERRIDE;
  virtual OperationProgressStatusList GetProgressStatusList() const OVERRIDE;
  virtual bool HasAccessToken() const OVERRIDE;
  virtual bool HasRefreshToken() const OVERRIDE;
  virtual void GetResourceList(
      const GURL& feed_url,
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
  virtual void GetApplicationInfo(const GetDataCallback& callback) OVERRIDE;
  virtual void DeleteResource(const GURL& edit_url,
                              const EntryActionCallback& callback) OVERRIDE;
  virtual void DownloadHostedDocument(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      DocumentExportFormat format,
      const DownloadActionCallback& callback) OVERRIDE;
  virtual void DownloadFile(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      const DownloadActionCallback& download_action_callback,
      const GetContentCallback& get_content_callback) OVERRIDE;
  // The new resource ID for the copied document will look like
  // |resource_id| + "_copied".
  virtual void CopyHostedDocument(
      const std::string& resource_id,
      const FilePath::StringType& new_name,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual void RenameResource(const GURL& edit_url,
                              const FilePath::StringType& new_name,
                              const EntryActionCallback& callback) OVERRIDE;
  virtual void AddResourceToDirectory(
      const GURL& parent_content_url,
      const GURL& edit_url,
      const EntryActionCallback& callback) OVERRIDE;
  virtual void RemoveResourceFromDirectory(
      const GURL& parent_content_url,
      const std::string& resource_id,
      const EntryActionCallback& callback) OVERRIDE;
  virtual void AddNewDirectory(
      const GURL& parent_content_url,
      const FilePath::StringType& directory_name,
      const GetResourceEntryCallback& callback) OVERRIDE;
  virtual void InitiateUpload(const InitiateUploadParams& params,
                              const InitiateUploadCallback& callback) OVERRIDE;
  virtual void ResumeUpload(const ResumeUploadParams& params,
                            const ResumeUploadCallback& callback) OVERRIDE;
  virtual void AuthorizeApp(const GURL& edit_url,
                            const std::string& app_id,
                            const AuthorizeAppCallback& callback) OVERRIDE;

 private:
  // Returns a pointer to the entry that matches |resource_id|, or NULL if
  // not found.
  base::DictionaryValue* FindEntryByResourceId(const std::string& resource_id);

  // Returns a pointer to the entry that matches |edit_url|, or NULL if not
  // found.
  base::DictionaryValue* FindEntryByEditUrl(const GURL& edit_url);

  // Returns a pointer to the entry that matches |content_url|, or NULL if
  // not found.
  base::DictionaryValue* FindEntryByContentUrl(const GURL& content_url);

  // Returns a new resource ID, which looks like "resource_id_<num>" where
  // <num> is a monotonically increasing number starting from 1.
  std::string GetNewResourceId();

  scoped_ptr<base::Value> resource_list_value_;
  scoped_ptr<base::Value> account_metadata_value_;
  scoped_ptr<base::Value> app_info_value_;
  int resource_id_count_;
  bool offline_;

  DISALLOW_COPY_AND_ASSIGN(FakeDriveService);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_FAKE_DRIVE_SERVICE_H_
