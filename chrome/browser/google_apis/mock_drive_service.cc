// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/mock_drive_service.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
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
  ON_CALL(*this, GetChangeList(_, _))
      .WillByDefault(Invoke(this, &MockDriveService::GetChangeListStub));
  ON_CALL(*this, GetAccountMetadata(_))
      .WillByDefault(Invoke(this, &MockDriveService::GetAccountMetadataStub));
  ON_CALL(*this, DeleteResource(_, _, _))
      .WillByDefault(Invoke(this, &MockDriveService::DeleteResourceStub));
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
  ON_CALL(*this, DownloadFile(_, _, _, _, _, _))
      .WillByDefault(Invoke(this, &MockDriveService::DownloadFileStub));

  // Fill in the default values for mock data.
  account_metadata_data_ =
      test_util::LoadJSONFile("chromeos/gdata/account_metadata.json");
  directory_data_ =
      test_util::LoadJSONFile("chromeos/gdata/new_folder_entry.json");
}

MockDriveService::~MockDriveService() {}

void MockDriveService::GetChangeListStub(
    int64 start_changestamp,
    const GetResourceListCallback& callback) {
  scoped_ptr<ResourceList> resource_list(new ResourceList());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS,
                 base::Passed(&resource_list)));
}

void MockDriveService::GetAccountMetadataStub(
    const GetAccountMetadataCallback& callback) {
  scoped_ptr<AccountMetadata> account_metadata =
      AccountMetadata::CreateFrom(*account_metadata_data_);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS,
                 base::Passed(&account_metadata)));
}

void MockDriveService::DeleteResourceStub(
    const std::string& resource_id,
    const std::string& etag,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS));
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
    const std::string& resource_id,
    const std::string& new_name,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS));
}

void MockDriveService::AddResourceToDirectoryStub(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS));
}

void MockDriveService::RemoveResourceFromDirectoryStub(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS));
}

void MockDriveService::CreateDirectoryStub(
    const std::string& parent_resource_id,
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
    const base::FilePath& virtual_path,
    const base::FilePath& local_tmp_path,
    const GURL& download_url,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const ProgressCallback& progress_callback) {
  GDataErrorCode error = HTTP_SUCCESS;
  if (file_data_.get()) {
    ASSERT_TRUE(test_util::WriteStringToFile(local_tmp_path, *file_data_));
  }
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(download_action_callback, error, local_tmp_path));
}

}  // namespace google_apis
