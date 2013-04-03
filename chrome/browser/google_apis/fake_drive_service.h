// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_FAKE_DRIVE_SERVICE_H_
#define CHROME_BROWSER_GOOGLE_APIS_FAKE_DRIVE_SERVICE_H_

#include "base/files/file_path.h"
#include "base/values.h"
#include "chrome/browser/google_apis/drive_service_interface.h"

namespace google_apis {

// This class implements a fake DriveService which acts like a real Drive
// service. The fake service works as follows:
//
// 1) Load JSON files and construct the in-memory resource list.
// 2) Return valid responses based on the the in-memory resource list.
// 3) Update the in-memory resource list by operations like DeleteResource().
class FakeDriveService : public DriveServiceInterface {
 public:
  FakeDriveService();
  virtual ~FakeDriveService();

  // Loads the resource list for WAPI. Returns true on success.
  bool LoadResourceListForWapi(const std::string& relative_path);

  // Loads the account metadata for WAPI. Returns true on success.  Also adds
  // the largest changestamp in the account metadata to the existing
  // entries. The changestamp information will be used to generate change
  // lists in GetResourceList() when non-zero |start_changestamp| is
  // specified.
  bool LoadAccountMetadataForWapi(const std::string& relative_path);

  // Loads the app list for Drive API. Returns true on success.
  bool LoadAppListForDriveApi(const std::string& relative_path);

  // Changes the offline state. All functions fail with GDATA_NO_CONNECTION
  // when offline. By default the offline state is false.
  void set_offline(bool offline) { offline_ = offline; }

  // Changes the default max results returned from GetResourceList().
  // By default, it's set to 0, which is unlimited.
  void set_default_max_results(int default_max_results) {
    default_max_results_ = default_max_results;
  }

  // Returns the largest changestamp, which starts from 0 by default. See
  // also comments at LoadAccountMetadataForWapi().
  int64 largest_changestamp() const { return largest_changestamp_; }

  // Returns the number of times the resource list is successfully loaded by
  // GetResourceList().
  int resource_list_load_count() const { return resource_list_load_count_; }

  // Returns the number of times the account metadata is successfully loaded
  // by GetAccountMetadata().
  int account_metadata_load_count() const {
    return account_metadata_load_count_;
  }

  // Returns the number of times the about resource is successfully loaded
  // by GetAboutResource().
  int about_resource_load_count() const {
    return about_resource_load_count_;
  }

  // Returns the file path whose operation is cancelled just before this method
  // invocation.
  const base::FilePath& last_cancelled_file() const {
    return last_cancelled_file_;
  }

  // Returns the (fake) URL for the link.
  static GURL GetFakeLinkUrl(const std::string& resource_id);

  // DriveServiceInterface Overrides
  virtual void Initialize(Profile* profile) OVERRIDE;
  virtual void AddObserver(DriveServiceObserver* observer) OVERRIDE;
  virtual void RemoveObserver(DriveServiceObserver* observer) OVERRIDE;
  virtual bool CanStartOperation() const OVERRIDE;
  virtual void CancelAll() OVERRIDE;
  virtual bool CancelForFilePath(const base::FilePath& file_path) OVERRIDE;
  virtual OperationProgressStatusList GetProgressStatusList() const OVERRIDE;
  virtual std::string GetRootResourceId() const OVERRIDE;
  virtual bool HasAccessToken() const OVERRIDE;
  virtual bool HasRefreshToken() const OVERRIDE;
  virtual void ClearAccessToken() OVERRIDE;
  virtual void ClearRefreshToken() OVERRIDE;
  // See the comment for EntryMatchWidthQuery() in .cc file for details about
  // the supported search query types.
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
  // The new resource ID for the copied document will look like
  // |resource_id| + "_copied".
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

 private:
  // Returns a pointer to the entry that matches |resource_id|, or NULL if
  // not found.
  base::DictionaryValue* FindEntryByResourceId(const std::string& resource_id);

  // Returns a pointer to the entry that matches |content_url|, or NULL if
  // not found.
  base::DictionaryValue* FindEntryByContentUrl(const GURL& content_url);

  // Returns a pointer to the entry that matches |upload_url|, or NULL if
  // not found.
  base::DictionaryValue* FindEntryByUploadUrl(const GURL& upload_url);

  // Returns a new resource ID, which looks like "resource_id_<num>" where
  // <num> is a monotonically increasing number starting from 1.
  std::string GetNewResourceId();

  // Increments |largest_changestamp_| and adds the new changestamp to
  // |entry|.
  void AddNewChangestamp(base::DictionaryValue* entry);

  // Adds a new entry based on the given parameters. |entry_kind| should be
  // "file" or "folder". Returns a pointer to the newly added entry, or NULL
  // if failed.
  const base::DictionaryValue* AddNewEntry(
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const std::string& entry_kind);

  scoped_ptr<base::Value> resource_list_value_;
  scoped_ptr<base::Value> account_metadata_value_;
  scoped_ptr<base::Value> app_info_value_;
  int64 largest_changestamp_;
  int default_max_results_;
  int resource_id_count_;
  int resource_list_load_count_;
  int account_metadata_load_count_;
  int about_resource_load_count_;
  bool offline_;
  base::FilePath last_cancelled_file_;

  DISALLOW_COPY_AND_ASSIGN(FakeDriveService);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_FAKE_DRIVE_SERVICE_H_
