// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/mock_drive_service.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "chrome/browser/chromeos/drive/drive_test_util.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace drive {

MockDriveService::MockDriveService() {
  ON_CALL(*this, GetProgressStatusList())
      .WillByDefault(Return(google_apis::OperationProgressStatusList()));
  ON_CALL(*this, Authenticate(_))
      .WillByDefault(Invoke(this, &MockDriveService::AuthenticateStub));
  ON_CALL(*this, GetDocuments(_, _, _, _, _))
      .WillByDefault(Invoke(this, &MockDriveService::GetDocumentsStub));
  ON_CALL(*this, GetAccountMetadata(_))
      .WillByDefault(Invoke(this,
                            &MockDriveService::GetAccountMetadataStub));
  ON_CALL(*this, DeleteDocument(_, _))
      .WillByDefault(Invoke(this, &MockDriveService::DeleteDocumentStub));
  ON_CALL(*this, DownloadDocument(_, _, _, _, _))
      .WillByDefault(Invoke(this, &MockDriveService::DownloadDocumentStub));
  ON_CALL(*this, CopyDocument(_, _, _))
      .WillByDefault(Invoke(this, &MockDriveService::CopyDocumentStub));
  ON_CALL(*this, RenameResource(_, _, _))
      .WillByDefault(Invoke(this, &MockDriveService::RenameResourceStub));
  ON_CALL(*this, AddResourceToDirectory(_, _, _))
      .WillByDefault(
          Invoke(this, &MockDriveService::AddResourceToDirectoryStub));
  ON_CALL(*this, RemoveResourceFromDirectory(_, _, _, _))
      .WillByDefault(
          Invoke(this, &MockDriveService::RemoveResourceFromDirectoryStub));
  ON_CALL(*this, CreateDirectory(_, _, _))
      .WillByDefault(Invoke(this, &MockDriveService::CreateDirectoryStub));
  ON_CALL(*this, DownloadFile(_, _, _, _, _))
      .WillByDefault(Invoke(this, &MockDriveService::DownloadFileStub));

  // Fill in the default values for mock feeds.
  account_metadata_ =
      google_apis::test_util::LoadJSONFile("gdata/account_metadata.json");
  feed_data_ = google_apis::test_util::LoadJSONFile("gdata/basic_feed.json");
  directory_data_ =
      google_apis::test_util::LoadJSONFile("gdata/new_folder_entry.json");
}

MockDriveService::~MockDriveService() {}

void MockDriveService::set_search_result(
    const std::string& search_result_feed) {
  search_result_ = google_apis::test_util::LoadJSONFile(search_result_feed);
}

void MockDriveService::AuthenticateStub(
    const google_apis::AuthStatusCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, google_apis::HTTP_SUCCESS, "my_auth_token"));
}

void MockDriveService::GetDocumentsStub(
    const GURL& feed_url,
    int64 start_changestamp,
    const std::string& search_string,
    const std::string& directory_resource_id,
    const google_apis::GetDataCallback& callback) {
  if (search_string.empty()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, google_apis::HTTP_SUCCESS,
                   base::Passed(&feed_data_)));
  } else {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, google_apis::HTTP_SUCCESS,
                   base::Passed(&search_result_)));
  }
}

void MockDriveService::GetAccountMetadataStub(
    const google_apis::GetDataCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, google_apis::HTTP_SUCCESS,
                 base::Passed(&account_metadata_)));
}

void MockDriveService::DeleteDocumentStub(
    const GURL& document_url,
    const google_apis::EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, google_apis::HTTP_SUCCESS, document_url));
}

void MockDriveService::DownloadDocumentStub(
    const FilePath& virtual_path,
    const FilePath& local_tmp_path,
    const GURL& content_url,
    google_apis::DocumentExportFormat format,
    const google_apis::DownloadActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, google_apis::HTTP_SUCCESS,
                 content_url, local_tmp_path));
}

void MockDriveService::CopyDocumentStub(
    const std::string& resource_id,
    const FilePath::StringType& new_name,
    const google_apis::GetDataCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, google_apis::HTTP_SUCCESS,
                 base::Passed(&document_data_)));
}

void MockDriveService::RenameResourceStub(
    const GURL& resource_url,
    const FilePath::StringType& new_name,
    const google_apis::EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, google_apis::HTTP_SUCCESS, resource_url));
}

void MockDriveService::AddResourceToDirectoryStub(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const google_apis::EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, google_apis::HTTP_SUCCESS, resource_url));
}

void MockDriveService::RemoveResourceFromDirectoryStub(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const std::string& resource_id,
    const google_apis::EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, google_apis::HTTP_SUCCESS, resource_url));
}

void MockDriveService::CreateDirectoryStub(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const google_apis::GetDataCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, google_apis::HTTP_SUCCESS,
                 base::Passed(&directory_data_)));
}

void MockDriveService::DownloadFileStub(
    const FilePath& virtual_path,
    const FilePath& local_tmp_path,
    const GURL& content_url,
    const google_apis::DownloadActionCallback& download_action_callback,
    const google_apis::GetContentCallback& get_content_callback) {
  google_apis::GDataErrorCode error = google_apis::HTTP_SUCCESS;
  if (file_data_.get()) {
    int file_data_size = static_cast<int>(file_data_->size());
    ASSERT_EQ(file_data_size,
              file_util::WriteFile(local_tmp_path, file_data_->data(),
                                   file_data_size));
  }
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(download_action_callback, error, content_url, local_tmp_path));
}

}  // namespace drive
