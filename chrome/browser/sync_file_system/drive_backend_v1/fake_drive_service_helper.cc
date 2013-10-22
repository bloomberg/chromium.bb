// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend_v1/fake_drive_service_helper.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/api_util.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/file_system_url.h"

#define FPL(path) FILE_PATH_LITERAL(path)

using google_apis::AboutResource;
using google_apis::GDataErrorCode;
using google_apis::ResourceEntry;
using google_apis::ResourceList;

namespace sync_file_system {
namespace drive_backend {

namespace {

void UploadResultCallback(GDataErrorCode* error_out,
                          scoped_ptr<ResourceEntry>* entry_out,
                          GDataErrorCode error,
                          const GURL& upload_location,
                          scoped_ptr<ResourceEntry> entry) {
  ASSERT_TRUE(error_out);
  ASSERT_TRUE(entry_out);
  *error_out = error;
  *entry_out = entry.Pass();
}

void DownloadResultCallback(GDataErrorCode* error_out,
                            GDataErrorCode error,
                            const base::FilePath& local_file) {
  ASSERT_TRUE(error_out);
  *error_out = error;
}

}  // namespace

FakeDriveServiceHelper::FakeDriveServiceHelper(
    drive::FakeDriveService* fake_drive_service,
    drive::DriveUploaderInterface* drive_uploader)
    : fake_drive_service_(fake_drive_service),
      drive_uploader_(drive_uploader) {
  Initialize();
}

FakeDriveServiceHelper::~FakeDriveServiceHelper() {
}

GDataErrorCode FakeDriveServiceHelper::AddOrphanedFolder(
    const std::string& title,
    std::string* folder_id) {
  EXPECT_TRUE(folder_id);

  std::string root_folder_id = fake_drive_service_->GetRootResourceId();
  GDataErrorCode error = AddFolder(root_folder_id, title, folder_id);
  if (error != google_apis::HTTP_CREATED)
    return error;

  error = google_apis::GDATA_OTHER_ERROR;
  fake_drive_service_->RemoveResourceFromDirectory(
      root_folder_id, *folder_id,
      CreateResultReceiver(&error));
  base::RunLoop().RunUntilIdle();

  if (error != google_apis::HTTP_SUCCESS)
    return error;
  return google_apis::HTTP_CREATED;
}

GDataErrorCode FakeDriveServiceHelper::AddFolder(
    const std::string& parent_folder_id,
    const std::string& title,
    std::string* folder_id) {
  EXPECT_TRUE(folder_id);

  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> folder;
  fake_drive_service_->AddNewDirectory(
      parent_folder_id, title,
      CreateResultReceiver(&error, &folder));
  base::RunLoop().RunUntilIdle();

  if (error == google_apis::HTTP_CREATED)
    *folder_id = folder->resource_id();
  return error;
}

GDataErrorCode FakeDriveServiceHelper::AddFile(
    const std::string& parent_folder_id,
    const std::string& title,
    const std::string& content,
    std::string* file_id) {
  EXPECT_TRUE(file_id);
  base::FilePath temp_file = WriteToTempFile(content);

  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> file;
  drive_uploader_->UploadNewFile(
      parent_folder_id, temp_file, title,
      "application/octet-stream",
      base::Bind(&UploadResultCallback, &error, &file),
      google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

  if (error == google_apis::HTTP_SUCCESS)
    *file_id = file->resource_id();
  return error;
}

GDataErrorCode FakeDriveServiceHelper::UpdateFile(
    const std::string& file_id,
    const std::string& content) {
  base::FilePath temp_file = WriteToTempFile(content);
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> file;
  drive_uploader_->UploadExistingFile(
      file_id, temp_file,
      "application/octet-stream",
      std::string(),  // etag
      base::Bind(&UploadResultCallback, &error, &file),
      google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();
  return error;
}

GDataErrorCode FakeDriveServiceHelper::RemoveResource(
    const std::string& file_id) {
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  fake_drive_service_->DeleteResource(
      file_id,
      std::string(),  // etag
      CreateResultReceiver(&error));
  base::RunLoop().RunUntilIdle();
  return error;
}

GDataErrorCode FakeDriveServiceHelper::GetSyncRootFolderID(
    std::string* sync_root_folder_id) {
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_drive_service_->SearchByTitle(
      APIUtil::GetSyncRootDirectoryName(), std::string(),
      CreateResultReceiver(&error, &resource_list));
  base::RunLoop().RunUntilIdle();
  if (error != google_apis::HTTP_SUCCESS)
    return error;

  const ScopedVector<ResourceEntry>& entries = resource_list->entries();
  for (ScopedVector<ResourceEntry>::const_iterator itr = entries.begin();
       itr != entries.end(); ++itr) {
    const ResourceEntry& entry = **itr;
    if (!entry.GetLinkByType(google_apis::Link::LINK_PARENT)) {
      *sync_root_folder_id = entry.resource_id();
      return google_apis::HTTP_SUCCESS;
    }
  }
  return google_apis::HTTP_NOT_FOUND;
}

GDataErrorCode FakeDriveServiceHelper::ListFilesInFolder(
    const std::string& folder_id,
    ScopedVector<ResourceEntry>* entries) {
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> list;
  fake_drive_service_->GetResourceListInDirectory(
      folder_id,
      CreateResultReceiver(&error, &list));
  base::RunLoop().RunUntilIdle();
  if (error != google_apis::HTTP_SUCCESS)
    return error;

  return CompleteListing(list.Pass(), entries);
}

GDataErrorCode FakeDriveServiceHelper::SearchByTitle(
    const std::string& folder_id,
    const std::string& title,
    ScopedVector<ResourceEntry>* entries) {
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> list;
  fake_drive_service_->SearchByTitle(
      title, folder_id,
      CreateResultReceiver(&error, &list));
  base::RunLoop().RunUntilIdle();
  if (error != google_apis::HTTP_SUCCESS)
    return error;

  return CompleteListing(list.Pass(), entries);
}

GDataErrorCode FakeDriveServiceHelper::GetResourceEntry(
    const std::string& file_id,
    scoped_ptr<ResourceEntry>* entry) {
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  fake_drive_service_->GetResourceEntry(
      file_id,
      CreateResultReceiver(&error, entry));
  base::RunLoop().RunUntilIdle();
  return error;
}

GDataErrorCode FakeDriveServiceHelper::ReadFile(
    const std::string& file_id,
    std::string* file_content) {
  scoped_ptr<google_apis::ResourceEntry> file;
  GDataErrorCode error = GetResourceEntry(file_id, &file);
  if (error != google_apis::HTTP_SUCCESS)
    return error;
  if (!file)
    return google_apis::GDATA_PARSE_ERROR;

  error = google_apis::GDATA_OTHER_ERROR;
  base::FilePath temp_file;
  EXPECT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_, &temp_file));
  fake_drive_service_->DownloadFile(
      temp_file, file->resource_id(),
      base::Bind(&DownloadResultCallback, &error),
      google_apis::GetContentCallback(),
      google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();
  if (error != google_apis::HTTP_SUCCESS)
    return error;

  return base::ReadFileToString(temp_file, file_content)
      ? google_apis::HTTP_SUCCESS : google_apis::GDATA_FILE_ERROR;
}

GDataErrorCode FakeDriveServiceHelper::GetAboutResource(
    scoped_ptr<AboutResource>* about_resource) {
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  fake_drive_service_->GetAboutResource(
      CreateResultReceiver(&error, about_resource));
  base::RunLoop().RunUntilIdle();
  return error;
}

GDataErrorCode FakeDriveServiceHelper::CompleteListing(
    scoped_ptr<ResourceList> list,
    ScopedVector<ResourceEntry>* entries) {
  while (true) {
    entries->reserve(entries->size() + list->entries().size());
    for (ScopedVector<ResourceEntry>::iterator itr =
         list->mutable_entries()->begin();
         itr != list->mutable_entries()->end(); ++itr) {
      entries->push_back(*itr);
      *itr = NULL;
    }

    GURL next_feed;
    if (!list->GetNextFeedURL(&next_feed))
      return google_apis::HTTP_SUCCESS;

    GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    list.reset();
    fake_drive_service_->GetRemainingFileList(
        next_feed,
        CreateResultReceiver(&error, &list));
    base::RunLoop().RunUntilIdle();
    if (error != google_apis::HTTP_SUCCESS)
      return error;
  }
}

void FakeDriveServiceHelper::Initialize() {
  ASSERT_TRUE(base_dir_.CreateUniqueTempDir());
  temp_dir_ = base_dir_.path().Append(FPL("tmp"));
  ASSERT_TRUE(file_util::CreateDirectory(temp_dir_));
}

base::FilePath FakeDriveServiceHelper::WriteToTempFile(
    const std::string& content) {
  base::FilePath temp_file;
  EXPECT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_, &temp_file));
  EXPECT_EQ(static_cast<int>(content.size()),
            file_util::WriteFile(temp_file, content.data(), content.size()));
  return temp_file;
}

}  // namespace drive_backend
}  // namespace sync_file_system
