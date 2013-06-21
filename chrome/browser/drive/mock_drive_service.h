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

namespace drive {

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
  MOCK_CONST_METHOD1(CanonicalizeResourceId,
                     std::string(const std::string& resource_id));
  MOCK_CONST_METHOD0(GetRootResourceId, std::string());
  MOCK_METHOD1(GetAllResourceList,
               google_apis::CancelCallback(
                   const google_apis::GetResourceListCallback& callback));
  MOCK_METHOD2(GetResourceListInDirectory,
               google_apis::CancelCallback(
                   const std::string& directory_resource_id,
                   const google_apis::GetResourceListCallback& callback));
  MOCK_METHOD2(Search,
               google_apis::CancelCallback(
                   const std::string& search_query,
                   const google_apis::GetResourceListCallback& callback));
  MOCK_METHOD3(SearchByTitle,
               google_apis::CancelCallback(
                   const std::string& title,
                   const std::string& directory_resource_id,
                   const google_apis::GetResourceListCallback& callback));
  MOCK_METHOD2(GetChangeList,
               google_apis::CancelCallback(
                   int64 start_changestamp,
                   const google_apis::GetResourceListCallback& callback));
  MOCK_METHOD2(ContinueGetResourceList,
               google_apis::CancelCallback(
                   const GURL& override_url,
                   const google_apis::GetResourceListCallback& callback));
  MOCK_METHOD2(GetResourceEntry,
               google_apis::CancelCallback(
                   const std::string& resource_id,
                   const google_apis::GetResourceEntryCallback& callback));
  MOCK_METHOD1(GetAboutResource,
               google_apis::CancelCallback(
                   const google_apis::GetAboutResourceCallback& callback));
  MOCK_METHOD1(GetAppList,
               google_apis::CancelCallback(
                   const google_apis::GetAppListCallback& callback));
  MOCK_METHOD3(DeleteResource,
               google_apis::CancelCallback(
                   const std::string& resource_id,
                   const std::string& etag,
                   const google_apis::EntryActionCallback& callback));
  MOCK_METHOD4(CopyResource,
               google_apis::CancelCallback(
                   const std::string& resource_id,
                   const std::string& parent_resource_id,
                   const std::string& new_name,
                   const google_apis::GetResourceEntryCallback& callback));
  MOCK_METHOD3(CopyHostedDocument,
               google_apis::CancelCallback(
                   const std::string& resource_id,
                   const std::string& new_name,
                   const google_apis::GetResourceEntryCallback& callback));
  MOCK_METHOD3(RenameResource,
               google_apis::CancelCallback(
                   const std::string& resource_id,
                   const std::string& new_name,
                   const google_apis::EntryActionCallback& callback));
  MOCK_METHOD4(TouchResource,
               google_apis::CancelCallback(
                   const std::string& resource_id,
                   const base::Time& modified_date,
                   const base::Time& last_viewed_by_me_date,
                   const google_apis::GetResourceEntryCallback& callback));
  MOCK_METHOD3(AddResourceToDirectory,
               google_apis::CancelCallback(
                   const std::string& parent_resource_id,
                   const std::string& resource_id,
                   const google_apis::EntryActionCallback& callback));
  MOCK_METHOD3(RemoveResourceFromDirectory,
               google_apis::CancelCallback(
                   const std::string& parent_resource_id,
                   const std::string& resource_id,
                   const google_apis::EntryActionCallback& callback));
  MOCK_METHOD3(AddNewDirectory,
               google_apis::CancelCallback(
                   const std::string& parent_resource_id,
                   const std::string& directory_name,
                   const google_apis::GetResourceEntryCallback& callback));
  MOCK_METHOD5(
      DownloadFile,
      google_apis::CancelCallback(
          const base::FilePath& local_cache_path,
          const GURL& download_url,
          const google_apis::DownloadActionCallback& donwload_action_callback,
          const google_apis::GetContentCallback& get_content_callback,
          const google_apis::ProgressCallback& progress_callback));
  MOCK_METHOD5(InitiateUploadNewFile,
               google_apis::CancelCallback(
                   const std::string& content_type,
                   int64 content_length,
                   const std::string& parent_resource_id,
                   const std::string& title,
                   const google_apis::InitiateUploadCallback& callback));
  MOCK_METHOD5(InitiateUploadExistingFile,
               google_apis::CancelCallback(
                   const std::string& content_type,
                   int64 content_length,
                   const std::string& resource_id,
                   const std::string& etag,
                   const google_apis::InitiateUploadCallback& callback));
  MOCK_METHOD8(ResumeUpload,
               google_apis::CancelCallback(
                   const GURL& upload_url,
                   int64 start_position,
                   int64 end_position,
                   int64 content_length,
                   const std::string& content_type,
                   const base::FilePath& local_file_path,
                   const google_apis::UploadRangeCallback& callback,
                   const google_apis::ProgressCallback& progress_callback));
  MOCK_METHOD3(GetUploadStatus,
               google_apis::CancelCallback(
                   const GURL& upload_url,
                   int64 content_length,
                   const google_apis::UploadRangeCallback& callback));
  MOCK_METHOD3(AuthorizeApp,
               google_apis::CancelCallback(
                   const std::string& resource_id,
                   const std::string& app_id,
                   const google_apis::AuthorizeAppCallback& callback));
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
  google_apis::CancelCallback GetChangeListStub(
      int64 start_changestamp,
      const google_apis::GetResourceListCallback& callback);

  // Will call |callback| with HTTP_SUCCESS.
  google_apis::CancelCallback DeleteResourceStub(
      const std::string& resource_id,
      const std::string& etag,
      const google_apis::EntryActionCallback& callback);

  // Will call |callback| with HTTP_SUCCESS and the current value of
  // |document_data_|.
  google_apis::CancelCallback CopyHostedDocumentStub(
      const std::string& resource_id,
      const std::string& new_name,
      const google_apis::GetResourceEntryCallback& callback);

  // Will call |callback| with HTTP_SUCCESS.
  google_apis::CancelCallback RenameResourceStub(
      const std::string& resource_id,
      const std::string& new_name,
      const google_apis::EntryActionCallback& callback);

  // Will call |callback| with HTTP_SUCCESS.
  google_apis::CancelCallback AddResourceToDirectoryStub(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback);

  // Will call |callback| with HTTP_SUCCESS.
  google_apis::CancelCallback RemoveResourceFromDirectoryStub(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback);

  // Will call |callback| with HTTP_SUCCESS and the current value of
  // |directory_data_|.
  google_apis::CancelCallback CreateDirectoryStub(
      const std::string& parent_resource_id,
      const std::string& directory_name,
      const google_apis::GetResourceEntryCallback& callback);

  // Will call |callback| with HTTP_SUCCESS, the given URL, and the host+path
  // portion of the URL as the temporary file path. If |file_data_| is not null,
  // |file_data_| is written to the temporary file.
  google_apis::CancelCallback DownloadFileStub(
      const base::FilePath& local_tmp_path,
      const GURL& download_url,
      const google_apis::DownloadActionCallback& download_action_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const google_apis::ProgressCallback& progress_callback);

  // JSON data to be returned from CreateDirectory.
  scoped_ptr<base::Value> directory_data_;

  // JSON data to be returned from CopyHostedDocument.
  scoped_ptr<base::Value> document_data_;

  // File data to be written to the local temporary file when
  // DownloadFileStub is called.
  scoped_ptr<std::string> file_data_;
};

}  // namespace drive

#endif  // CHROME_BROWSER_DRIVE_MOCK_DRIVE_SERVICE_H_
