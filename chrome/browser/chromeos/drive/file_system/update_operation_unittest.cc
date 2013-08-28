// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/update_operation.h"

#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {

class UpdateOperationTest : public OperationTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
   OperationTestBase::SetUp();
   operation_.reset(new UpdateOperation(blocking_task_runner(),
                                        observer(),
                                        scheduler(),
                                        metadata(),
                                        cache()));
 }

 scoped_ptr<UpdateOperation> operation_;
};

TEST_F(UpdateOperationTest, UpdateFileByLocalId_PersistentFile) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  const std::string kResourceId("file:2_file_resource_id");
  const std::string kMd5("3b4382ebefec6e743578c76bbd0575ce");

  const base::FilePath kTestFile = temp_dir().Append(FILE_PATH_LITERAL("foo"));
  const std::string kTestFileContent = "I'm being uploaded! Yay!";
  google_apis::test_util::WriteStringToFile(kTestFile, kTestFileContent);

  // Pin the file so it'll be store in "persistent" directory.
  FileError error = FILE_ERROR_FAILED;
  cache()->PinOnUIThread(
      kResourceId,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // First store a file to cache.
  error = FILE_ERROR_FAILED;
  cache()->StoreOnUIThread(
      kResourceId, kMd5, kTestFile,
      internal::FileCache::FILE_OPERATION_COPY,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Add the dirty bit.
  error = FILE_ERROR_FAILED;
  cache()->MarkDirtyOnUIThread(
      kResourceId,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  int64 original_changestamp = fake_service()->largest_changestamp();

  // The callback will be called upon completion of UpdateFileByLocalId().
  error = FILE_ERROR_FAILED;
  operation_->UpdateFileByLocalId(
      kResourceId,
      ClientContext(USER_INITIATED),
      UpdateOperation::RUN_CONTENT_CHECK,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Check that the server has received an update.
  EXPECT_LT(original_changestamp, fake_service()->largest_changestamp());

  // Check that the file size is updated to that of the updated content.
  google_apis::GDataErrorCode gdata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> server_entry;
  fake_service()->GetResourceEntry(
      kResourceId,
      google_apis::test_util::CreateCopyResultCallback(&gdata_error,
                                                       &server_entry));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, gdata_error);
  EXPECT_EQ(static_cast<int64>(kTestFileContent.size()),
            server_entry->file_size());

  // Make sure that the cache is no longer dirty.
  bool success = false;
  FileCacheEntry cache_entry;
  cache()->GetCacheEntryOnUIThread(
      server_entry->resource_id(),
      google_apis::test_util::CreateCopyResultCallback(&success, &cache_entry));
  test_util::RunBlockingPoolTask();
  ASSERT_TRUE(success);
  EXPECT_FALSE(cache_entry.is_dirty());
}

TEST_F(UpdateOperationTest, UpdateFileByLocalId_NonexistentFile) {
  FileError error = FILE_ERROR_OK;
  operation_->UpdateFileByLocalId(
      "file:nonexistent_resource_id",
      ClientContext(USER_INITIATED),
      UpdateOperation::RUN_CONTENT_CHECK,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
}

TEST_F(UpdateOperationTest, UpdateFileByLocalId_Md5) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  const std::string kResourceId("file:2_file_resource_id");
  const std::string kMd5("3b4382ebefec6e743578c76bbd0575ce");

  const base::FilePath kTestFile = temp_dir().Append(FILE_PATH_LITERAL("foo"));
  const std::string kTestFileContent = "I'm being uploaded! Yay!";
  google_apis::test_util::WriteStringToFile(kTestFile, kTestFileContent);

  // First store a file to cache.
  FileError error = FILE_ERROR_FAILED;
  cache()->StoreOnUIThread(
      kResourceId, kMd5, kTestFile,
      internal::FileCache::FILE_OPERATION_COPY,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Add the dirty bit.
  error = FILE_ERROR_FAILED;
  cache()->MarkDirtyOnUIThread(
      kResourceId,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  int64 original_changestamp = fake_service()->largest_changestamp();

  // The callback will be called upon completion of UpdateFileByLocalId().
  error = FILE_ERROR_FAILED;
  operation_->UpdateFileByLocalId(
      kResourceId,
      ClientContext(USER_INITIATED),
      UpdateOperation::RUN_CONTENT_CHECK,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Check that the server has received an update.
  EXPECT_LT(original_changestamp, fake_service()->largest_changestamp());

  // Check that the file size is updated to that of the updated content.
  google_apis::GDataErrorCode gdata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> server_entry;
  fake_service()->GetResourceEntry(
      kResourceId,
      google_apis::test_util::CreateCopyResultCallback(&gdata_error,
                                                       &server_entry));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, gdata_error);
  EXPECT_EQ(static_cast<int64>(kTestFileContent.size()),
            server_entry->file_size());

  // Make sure that the cache is no longer dirty.
  bool success = false;
  FileCacheEntry cache_entry;
  cache()->GetCacheEntryOnUIThread(
      server_entry->resource_id(),
      google_apis::test_util::CreateCopyResultCallback(&success, &cache_entry));
  test_util::RunBlockingPoolTask();
  ASSERT_TRUE(success);
  EXPECT_FALSE(cache_entry.is_dirty());

  // Again mark the cache file dirty.
  error = FILE_ERROR_FAILED;
  cache()->MarkDirtyOnUIThread(
      kResourceId,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // And call UpdateFileByLocalId again.
  // In this case, although the file is marked as dirty, but the content
  // hasn't been changed. Thus, the actual uploading should be skipped.
  original_changestamp = fake_service()->largest_changestamp();
  error = FILE_ERROR_FAILED;
  operation_->UpdateFileByLocalId(
      kResourceId,
      ClientContext(USER_INITIATED),
      UpdateOperation::RUN_CONTENT_CHECK,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_EQ(original_changestamp, fake_service()->largest_changestamp());

  // Make sure that the cache is no longer dirty.
  success = false;
  cache()->GetCacheEntryOnUIThread(
      server_entry->resource_id(),
      google_apis::test_util::CreateCopyResultCallback(&success, &cache_entry));
  test_util::RunBlockingPoolTask();
  ASSERT_TRUE(success);
  EXPECT_FALSE(cache_entry.is_dirty());

  // Once again mark the cache file dirty.
  error = FILE_ERROR_FAILED;
  cache()->MarkDirtyOnUIThread(
      kResourceId,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // And call UpdateFileByLocalId again.
  // In this case, NO_CONTENT_CHECK is set, so the actual uploading should run
  // no matter the content is changed or not.
  original_changestamp = fake_service()->largest_changestamp();
  error = FILE_ERROR_FAILED;
  operation_->UpdateFileByLocalId(
      kResourceId,
      ClientContext(USER_INITIATED),
      UpdateOperation::NO_CONTENT_CHECK,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Make sure that the server is receiving a change.
  EXPECT_LE(original_changestamp, fake_service()->largest_changestamp());
}

}  // namespace file_system
}  // namespace drive
