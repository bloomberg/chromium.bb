// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/mock_drive_service.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/values.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;

using google_apis::CancelCallback;
using google_apis::DownloadActionCallback;
using google_apis::EntryActionCallback;
using google_apis::GDataErrorCode;
using google_apis::GDATA_FILE_ERROR;
using google_apis::GetContentCallback;
using google_apis::GetResourceEntryCallback;
using google_apis::GetResourceListCallback;
using google_apis::HTTP_SUCCESS;
using google_apis::ProgressCallback;
using google_apis::ResourceEntry;
using google_apis::ResourceList;
namespace test_util = google_apis::test_util;

namespace drive {

MockDriveService::MockDriveService() {
  ON_CALL(*this, GetChangeList(_, _))
      .WillByDefault(Invoke(this, &MockDriveService::GetChangeListStub));
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
  ON_CALL(*this, DownloadFile(_, _, _, _, _))
      .WillByDefault(Invoke(this, &MockDriveService::DownloadFileStub));

  // Fill in the default values for mock data.
  directory_data_ =
      test_util::LoadJSONFile("chromeos/gdata/new_folder_entry.json");
}

MockDriveService::~MockDriveService() {}

CancelCallback MockDriveService::GetChangeListStub(
    int64 start_changestamp,
    const GetResourceListCallback& callback) {
  scoped_ptr<ResourceList> resource_list(new ResourceList());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS,
                 base::Passed(&resource_list)));
  return CancelCallback();
}

CancelCallback MockDriveService::DeleteResourceStub(
    const std::string& resource_id,
    const std::string& etag,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS));
  return CancelCallback();
}

CancelCallback MockDriveService::CopyHostedDocumentStub(
    const std::string& resource_id,
    const std::string& new_name,
    const GetResourceEntryCallback& callback) {
  scoped_ptr<ResourceEntry> resource_entry =
      ResourceEntry::ExtractAndParse(*document_data_);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS,
                 base::Passed(&resource_entry)));
  return CancelCallback();
}

CancelCallback MockDriveService::RenameResourceStub(
    const std::string& resource_id,
    const std::string& new_name,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS));
  return CancelCallback();
}

CancelCallback MockDriveService::AddResourceToDirectoryStub(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS));
  return CancelCallback();
}

CancelCallback MockDriveService::RemoveResourceFromDirectoryStub(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS));
  return CancelCallback();
}

CancelCallback MockDriveService::CreateDirectoryStub(
    const std::string& parent_resource_id,
    const std::string& directory_name,
    const GetResourceEntryCallback& callback) {
  scoped_ptr<ResourceEntry> resource_entry =
      ResourceEntry::ExtractAndParse(*directory_data_);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS,
                 base::Passed(&resource_entry)));
  return CancelCallback();
}

CancelCallback MockDriveService::DownloadFileStub(
    const base::FilePath& local_tmp_path,
    const GURL& download_url,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const ProgressCallback& progress_callback) {
  GDataErrorCode error = HTTP_SUCCESS;
  if (file_data_.get()) {
    if (!test_util::WriteStringToFile(local_tmp_path, *file_data_))
      error = GDATA_FILE_ERROR;
  }
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(download_action_callback, error, local_tmp_path));
  return CancelCallback();
}

}  // namespace drive
