// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/mock_drive_service.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace google_apis {

MockDriveService::MockDriveService() {
  ON_CALL(*this, GetProgressStatusList())
      .WillByDefault(Return(OperationProgressStatusList()));
  ON_CALL(*this, GetResourceList(_, _, _, _, _, _))
      .WillByDefault(Invoke(this, &MockDriveService::GetResourceListStub));
  ON_CALL(*this, GetAccountMetadata(_))
      .WillByDefault(Invoke(this, &MockDriveService::GetAccountMetadataStub));
  ON_CALL(*this, DeleteResource(_, _))
      .WillByDefault(Invoke(this, &MockDriveService::DeleteResourceStub));
  ON_CALL(*this, DownloadHostedDocument(_, _, _, _, _))
      .WillByDefault(
          Invoke(this, &MockDriveService::DownloadHostedDocumentStub));
  ON_CALL(*this, CopyHostedDocument(_, _, _))
      .WillByDefault(Invoke(this, &MockDriveService::CopyHostedDocumentStub));
  ON_CALL(*this, RenameResource(_, _, _))
      .WillByDefault(Invoke(this, &MockDriveService::RenameResourceStub));
  ON_CALL(*this, AddResourceToDirectory(_, _, _))
      .WillByDefault(
          Invoke(this, &MockDriveService::AddResourceToDirectoryStub));
  ON_CALL(*this, RemoveResourceFromDirectory(_, _, _))
      .WillByDefault(
          Invoke(this, &MockDriveService::RemoveResourceFromDirectoryStub));
  ON_CALL(*this, AddNewDirectory(_, _, _))
      .WillByDefault(Invoke(this, &MockDriveService::CreateDirectoryStub));
  ON_CALL(*this, DownloadFile(_, _, _, _, _))
      .WillByDefault(Invoke(this, &MockDriveService::DownloadFileStub));

  // Fill in the default values for mock feeds.
  account_metadata_data_ =
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

void MockDriveService::GetResourceListStub(
    const GURL& feed_url,
    int64 start_changestamp,
    const std::string& search_string,
    bool shared_with_me,
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback) {
  if (search_string.empty()) {
    scoped_ptr<ResourceList> resource_list =
        ResourceList::ExtractAndParse(*feed_data_);
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS,
                   base::Passed(&resource_list)));
  } else {
    scoped_ptr<ResourceList> resource_list =
        ResourceList::ExtractAndParse(*search_result_);
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS,
                   base::Passed(&resource_list)));
  }
}

void MockDriveService::GetAccountMetadataStub(
    const GetAccountMetadataCallback& callback) {
  scoped_ptr<AccountMetadataFeed> account_metadata =
      AccountMetadataFeed::CreateFrom(*account_metadata_data_);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS,
                 base::Passed(&account_metadata)));
}

void MockDriveService::DeleteResourceStub(
    const GURL& edit_url,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS));
}

void MockDriveService::DownloadHostedDocumentStub(
    const FilePath& virtual_path,
    const FilePath& local_tmp_path,
    const GURL& content_url,
    DocumentExportFormat format,
    const DownloadActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, local_tmp_path));
}

void MockDriveService::CopyHostedDocumentStub(
    const std::string& resource_id,
    const std::string& new_name,
    const GetResourceEntryCallback& callback) {
  scoped_ptr<ResourceEntry> resource_entry =
      ResourceEntry::ExtractAndParse(*document_data_);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS,
                 base::Passed(&resource_entry)));
}

void MockDriveService::RenameResourceStub(
    const GURL& edit_url,
    const std::string& new_name,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS));
}

void MockDriveService::AddResourceToDirectoryStub(
    const GURL& parent_content_url,
    const GURL& edit_url,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS));
}

void MockDriveService::RemoveResourceFromDirectoryStub(
    const GURL& parent_content_url,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS));
}

void MockDriveService::CreateDirectoryStub(
    const GURL& parent_content_url,
    const std::string& directory_name,
    const GetResourceEntryCallback& callback) {
  scoped_ptr<ResourceEntry> resource_entry =
      ResourceEntry::ExtractAndParse(*directory_data_);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS,
                 base::Passed(&resource_entry)));
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
      base::Bind(download_action_callback, error, local_tmp_path));
}

}  // namespace google_apis
