// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains mocks for classes in drive_service_interface.h

#ifndef CHROME_BROWSER_DRIVE_MOCK_DRIVE_SERVICE_H_
#define CHROME_BROWSER_DRIVE_MOCK_DRIVE_SERVICE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class FilePath;
}

namespace google_apis {

class MockDriveService : public DriveServiceInterface {
 public:
  MockDriveService();
  virtual ~MockDriveService();

  // DriveServiceInterface overrides.
  MOCK_METHOD1(Initialize, void(Profile* profile));
  MOCK_METHOD1(AddObserver, void(DriveServiceObserver* observer));
  MOCK_METHOD1(RemoveObserver,
      void(DriveServiceObserver* observer));
  MOCK_CONST_METHOD0(CanSendRequest, bool());
  MOCK_METHOD0(CancelAll, void(void));
  MOCK_METHOD1(CancelForFilePath, bool(const base::FilePath& file_path));
  MOCK_CONST_METHOD1(CanonicalizeResourceId,
                     std::string(const std::string& resource_id));
  MOCK_CONST_METHOD0(GetRootResourceId, std::string());
  MOCK_METHOD1(GetAllResourceList,
      CancelCallback(const GetResourceListCallback& callback));
  MOCK_METHOD2(GetResourceListInDirectory,
      CancelCallback(const std::string& directory_resource_id,
                     const GetResourceListCallback& callback));
  MOCK_METHOD2(Search,
      CancelCallback(const std::string& search_query,
                     const GetResourceListCallback& callback));
  MOCK_METHOD3(SearchByTitle,
      CancelCallback(const std::string& title,
                     const std::string& directory_resource_id,
                     const GetResourceListCallback& callback));
  MOCK_METHOD2(GetChangeList,
      CancelCallback(int64 start_changestamp,
                    const GetResourceListCallback& callback));
  MOCK_METHOD2(ContinueGetResourceList,
      CancelCallback(const GURL& override_url,
                     const GetResourceListCallback& callback));
  MOCK_METHOD2(GetResourceEntry,
      CancelCallback(const std::string& resource_id,
                     const GetResourceEntryCallback& callback));
  MOCK_METHOD1(GetAboutResource,
      CancelCallback(const GetAboutResourceCallback& callback));
  MOCK_METHOD1(GetAppList,
      CancelCallback(const GetAppListCallback& callback));
  MOCK_METHOD3(DeleteResource,
      CancelCallback(const std::string& resource_id,
                     const std::string& etag,
                     const EntryActionCallback& callback));
  MOCK_METHOD4(CopyResource,
      CancelCallback(const std::string& resource_id,
                     const std::string& parent_resource_id,
                     const std::string& new_name,
                     const GetResourceEntryCallback& callback));
  MOCK_METHOD3(CopyHostedDocument,
      CancelCallback(const std::string& resource_id,
                     const std::string& new_name,
                     const GetResourceEntryCallback& callback));
  MOCK_METHOD3(RenameResource,
      CancelCallback(const std::string& resource_id,
                     const std::string& new_name,
                     const EntryActionCallback& callback));
  MOCK_METHOD4(TouchResource,
      CancelCallback(const std::string& resource_id,
                     const base::Time& modified_date,
                     const base::Time& last_viewed_by_me_date,
                     const GetResourceEntryCallback& callback));
  MOCK_METHOD3(AddResourceToDirectory,
      CancelCallback(const std::string& parent_resource_id,
                     const std::string& resource_id,
                     const EntryActionCallback& callback));
  MOCK_METHOD3(RemoveResourceFromDirectory,
      CancelCallback(const std::string& parent_resource_id,
                     const std::string& resource_id,
                     const EntryActionCallback& callback));
  MOCK_METHOD3(AddNewDirectory,
      CancelCallback(const std::string& parent_resource_id,
                     const std::string& directory_name,
                     const GetResourceEntryCallback& callback));
  MOCK_METHOD6(
      DownloadFile,
      CancelCallback(const base::FilePath& virtual_path,
                     const base::FilePath& local_cache_path,
                     const GURL& download_url,
                     const DownloadActionCallback& donwload_action_callback,
                     const GetContentCallback& get_content_callback,
                     const ProgressCallback& progress_callback));
  MOCK_METHOD6(InitiateUploadNewFile,
      CancelCallback(const base::FilePath& drive_file_path,
                     const std::string& content_type,
                     int64 content_length,
                     const std::string& parent_resource_id,
                     const std::string& title,
                     const InitiateUploadCallback& callback));
  MOCK_METHOD6(InitiateUploadExistingFile,
      CancelCallback(const base::FilePath& drive_file_path,
                     const std::string& content_type,
                     int64 content_length,
                     const std::string& resource_id,
                     const std::string& etag,
                     const InitiateUploadCallback& callback));
  MOCK_METHOD9(ResumeUpload,
      CancelCallback(const base::FilePath& drive_file_path,
                     const GURL& upload_url,
                     int64 start_position,
                     int64 end_position,
                     int64 content_length,
                     const std::string& content_type,
                     const base::FilePath& local_file_path,
                     const UploadRangeCallback& callback,
                     const ProgressCallback& progress_callback));
  MOCK_METHOD4(GetUploadStatus,
      CancelCallback(const base::FilePath& drive_file_path,
                     const GURL& upload_url,
                     int64 content_length,
                     const UploadRangeCallback& callback));
  MOCK_METHOD3(AuthorizeApp,
      CancelCallback(const std::string& resource_id,
                     const std::string& app_id,
                     const AuthorizeAppCallback& callback));
  MOCK_CONST_METHOD0(HasAccessToken, bool());
  MOCK_CONST_METHOD0(HasRefreshToken, bool());
  MOCK_METHOD0(ClearAccessToken, void());
  MOCK_METHOD0(ClearRefreshToken, void());

  void set_file_data(std::string* file_data) {
    file_data_.reset(file_data);
  }

 private:
  // Helper stub methods for functions which take callbacks, so that
  // the callbacks get called with testable results.

  // Will call |callback| with HTTP_SUCCESS and a empty ResourceList.
  CancelCallback GetChangeListStub(
      int64 start_changestamp,
      const GetResourceListCallback& callback);

  // Will call |callback| with HTTP_SUCCESS.
  CancelCallback DeleteResourceStub(
      const std::string& resource_id,
      const std::string& etag,
      const EntryActionCallback& callback);

  // Will call |callback| with HTTP_SUCCESS and the current value of
  // |document_data_|.
  CancelCallback CopyHostedDocumentStub(
      const std::string& resource_id,
      const std::string& new_name,
      const GetResourceEntryCallback& callback);

  // Will call |callback| with HTTP_SUCCESS.
  CancelCallback RenameResourceStub(
      const std::string& resource_id,
      const std::string& new_name,
      const EntryActionCallback& callback);

  // Will call |callback| with HTTP_SUCCESS.
  CancelCallback AddResourceToDirectoryStub(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const EntryActionCallback& callback);

  // Will call |callback| with HTTP_SUCCESS.
  CancelCallback RemoveResourceFromDirectoryStub(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const EntryActionCallback& callback);

  // Will call |callback| with HTTP_SUCCESS and the current value of
  // |directory_data_|.
  CancelCallback CreateDirectoryStub(
      const std::string& parent_resource_id,
      const std::string& directory_name,
      const GetResourceEntryCallback& callback);

  // Will call |callback| with HTTP_SUCCESS, the given URL, and the host+path
  // portion of the URL as the temporary file path. If |file_data_| is not null,
  // |file_data_| is written to the temporary file.
  CancelCallback DownloadFileStub(
      const base::FilePath& virtual_path,
      const base::FilePath& local_tmp_path,
      const GURL& download_url,
      const DownloadActionCallback& download_action_callback,
      const GetContentCallback& get_content_callback,
      const ProgressCallback& progress_callback);

  // JSON data to be returned from CreateDirectory.
  scoped_ptr<base::Value> directory_data_;

  // JSON data to be returned from CopyHostedDocument.
  scoped_ptr<base::Value> document_data_;

  // File data to be written to the local temporary file when
  // DownloadFileStub is called.
  scoped_ptr<std::string> file_data_;
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_DRIVE_MOCK_DRIVE_SERVICE_H_
