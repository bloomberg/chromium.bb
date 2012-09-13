// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/mock_drive_service.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "chrome/browser/chromeos/gdata/drive_test_util.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;

namespace gdata {

MockDriveService::MockDriveService() {
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
      test_util::LoadJSONFile("gdata/account_metadata.json");
  feed_data_ = test_util::LoadJSONFile("gdata/basic_feed.json");
  directory_data_ =
      test_util::LoadJSONFile("gdata/new_folder_entry.json");
}

MockDriveService::~MockDriveService() {}

void MockDriveService::set_search_result(
    const std::string& search_result_feed) {
  search_result_ = test_util::LoadJSONFile(search_result_feed);
}

void MockDriveService::AuthenticateStub(
    const AuthStatusCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, "my_auth_token"));
}

void MockDriveService::GetDocumentsStub(
    const GURL& feed_url,
    int64 start_changestamp,
    const std::string& search_string,
    const std::string& directory_resource_id,
    const GetDataCallback& callback) {
  if (search_string.empty()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, base::Passed(&feed_data_)));
  } else {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, base::Passed(&search_result_)));
  }
}

void MockDriveService::GetAccountMetadataStub(
    const GetDataCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, base::Passed(&account_metadata_)));
}

void MockDriveService::DeleteDocumentStub(
    const GURL& document_url,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, document_url));
}

void MockDriveService::DownloadDocumentStub(
    const FilePath& virtual_path,
    const FilePath& local_tmp_path,
    const GURL& content_url,
    DocumentExportFormat format,
    const DownloadActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, content_url, local_tmp_path));
}

void MockDriveService::CopyDocumentStub(
    const std::string& resource_id,
    const FilePath::StringType& new_name,
    const GetDataCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, base::Passed(&document_data_)));
}

void MockDriveService::RenameResourceStub(
    const GURL& resource_url,
    const FilePath::StringType& new_name,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, resource_url));
}

void MockDriveService::AddResourceToDirectoryStub(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, resource_url));
}

void MockDriveService::RemoveResourceFromDirectoryStub(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, resource_url));
}

void MockDriveService::CreateDirectoryStub(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const GetDataCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, base::Passed(&directory_data_)));
}

void MockDriveService::DownloadFileStub(
    const FilePath& virtual_path,
    const FilePath& local_tmp_path,
    const GURL& content_url,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback) {
  GDataErrorCode error = HTTP_SUCCESS;
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

}  // namespace gdata
