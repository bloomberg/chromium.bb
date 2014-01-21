// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/sync/content_update_performer.h"

#include "base/callback_helpers.h"
#include "base/file_util.h"
#include "base/md5.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {

class ContentUpdatePerformerTest : public OperationTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
    OperationTestBase::SetUp();
    performer_.reset(new ContentUpdatePerformer(blocking_task_runner(),
                                                scheduler(),
                                                metadata(),
                                                cache()));
  }

  // Stores |content| to the cache and mark it as dirty.
  FileError StoreAndMarkDirty(const std::string& local_id,
                              const std::string& content) {
    base::FilePath path;
    if (!base::CreateTemporaryFileInDir(temp_dir(), &path) ||
        !google_apis::test_util::WriteStringToFile(path, content))
      return FILE_ERROR_FAILED;

    // Store the file to cache.
    FileError error = FILE_ERROR_FAILED;
    base::PostTaskAndReplyWithResult(
        blocking_task_runner(),
        FROM_HERE,
        base::Bind(&internal::FileCache::Store,
                   base::Unretained(cache()),
                   local_id, base::MD5String(content), path,
                   internal::FileCache::FILE_OPERATION_COPY),
        google_apis::test_util::CreateCopyResultCallback(&error));
    test_util::RunBlockingPoolTask();
    if (error != FILE_ERROR_OK)
      return error;

    // Add the dirty bit.
    error = FILE_ERROR_FAILED;
    scoped_ptr<base::ScopedClosureRunner> file_closer;
    base::PostTaskAndReplyWithResult(
        blocking_task_runner(),
        FROM_HERE,
        base::Bind(&internal::FileCache::OpenForWrite,
                   base::Unretained(cache()),
                   local_id,
                   &file_closer),
        google_apis::test_util::CreateCopyResultCallback(&error));
    test_util::RunBlockingPoolTask();
    return error;
  }

  scoped_ptr<ContentUpdatePerformer> performer_;
};

TEST_F(ContentUpdatePerformerTest, UpdateFileByLocalId) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  const std::string kResourceId("file:2_file_resource_id");

  const std::string local_id = GetLocalId(kFilePath);
  EXPECT_FALSE(local_id.empty());

  const std::string kTestFileContent = "I'm being uploaded! Yay!";
  EXPECT_EQ(FILE_ERROR_OK, StoreAndMarkDirty(local_id, kTestFileContent));

  int64 original_changestamp =
      fake_service()->about_resource().largest_change_id();

  // The callback will be called upon completion of UpdateFileByLocalId().
  FileError error = FILE_ERROR_FAILED;
  performer_->UpdateFileByLocalId(
      local_id,
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Check that the server has received an update.
  EXPECT_LT(original_changestamp,
            fake_service()->about_resource().largest_change_id());

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
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::FileCache::GetCacheEntry,
                 base::Unretained(cache()),
                 local_id,
                 &cache_entry),
      google_apis::test_util::CreateCopyResultCallback(&success));
  test_util::RunBlockingPoolTask();
  ASSERT_TRUE(success);
  EXPECT_FALSE(cache_entry.is_dirty());
}

TEST_F(ContentUpdatePerformerTest, UpdateFileByLocalId_NonexistentFile) {
  FileError error = FILE_ERROR_OK;
  performer_->UpdateFileByLocalId(
      "nonexistent_local_id",
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
}

TEST_F(ContentUpdatePerformerTest, UpdateFileByLocalId_Md5) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  const std::string kResourceId("file:2_file_resource_id");

  const std::string local_id = GetLocalId(kFilePath);
  EXPECT_FALSE(local_id.empty());

  const std::string kTestFileContent = "I'm being uploaded! Yay!";
  EXPECT_EQ(FILE_ERROR_OK, StoreAndMarkDirty(local_id, kTestFileContent));

  int64 original_changestamp =
      fake_service()->about_resource().largest_change_id();

  // The callback will be called upon completion of UpdateFileByLocalId().
  FileError error = FILE_ERROR_FAILED;
  performer_->UpdateFileByLocalId(
      local_id,
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Check that the server has received an update.
  EXPECT_LT(original_changestamp,
            fake_service()->about_resource().largest_change_id());

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
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::FileCache::GetCacheEntry,
                 base::Unretained(cache()),
                 local_id,
                 &cache_entry),
      google_apis::test_util::CreateCopyResultCallback(&success));
  test_util::RunBlockingPoolTask();
  ASSERT_TRUE(success);
  EXPECT_FALSE(cache_entry.is_dirty());

  // Again mark the cache file dirty.
  scoped_ptr<base::ScopedClosureRunner> file_closer;
  error = FILE_ERROR_FAILED;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::FileCache::OpenForWrite,
                 base::Unretained(cache()),
                 local_id,
                 &file_closer),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  file_closer.reset();

  // And call UpdateFileByLocalId again.
  // In this case, although the file is marked as dirty, but the content
  // hasn't been changed. Thus, the actual uploading should be skipped.
  original_changestamp = fake_service()->about_resource().largest_change_id();
  error = FILE_ERROR_FAILED;
  performer_->UpdateFileByLocalId(
      local_id,
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_EQ(original_changestamp,
            fake_service()->about_resource().largest_change_id());

  // Make sure that the cache is no longer dirty.
  success = false;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::FileCache::GetCacheEntry,
                 base::Unretained(cache()),
                 local_id,
                 &cache_entry),
      google_apis::test_util::CreateCopyResultCallback(&success));
  test_util::RunBlockingPoolTask();
  ASSERT_TRUE(success);
  EXPECT_FALSE(cache_entry.is_dirty());
}

TEST_F(ContentUpdatePerformerTest, UpdateFileByLocalId_OpenedForWrite) {
  const base::FilePath kFilePath(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  const std::string kResourceId("file:2_file_resource_id");

  const std::string local_id = GetLocalId(kFilePath);
  EXPECT_FALSE(local_id.empty());

  const std::string kTestFileContent = "I'm being uploaded! Yay!";
  EXPECT_EQ(FILE_ERROR_OK, StoreAndMarkDirty(local_id, kTestFileContent));

  // Emulate a situation where someone is writing to the file.
  scoped_ptr<base::ScopedClosureRunner> file_closer;
  FileError error = FILE_ERROR_FAILED;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::FileCache::OpenForWrite,
                 base::Unretained(cache()),
                 local_id,
                 &file_closer),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Update. This should not clear the dirty bit.
  error = FILE_ERROR_FAILED;
  performer_->UpdateFileByLocalId(
      local_id,
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Make sure that the cache is still dirty.
  bool success = false;
  FileCacheEntry cache_entry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::FileCache::GetCacheEntry,
                 base::Unretained(cache()),
                 local_id,
                 &cache_entry),
      google_apis::test_util::CreateCopyResultCallback(&success));
  test_util::RunBlockingPoolTask();
  EXPECT_TRUE(success);
  EXPECT_TRUE(cache_entry.is_dirty());

  // Close the file.
  file_closer.reset();

  // Update. This should clear the dirty bit.
  error = FILE_ERROR_FAILED;
  performer_->UpdateFileByLocalId(
      local_id,
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Make sure that the cache is no longer dirty.
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&internal::FileCache::GetCacheEntry,
                 base::Unretained(cache()),
                 local_id,
                 &cache_entry),
      google_apis::test_util::CreateCopyResultCallback(&success));
  test_util::RunBlockingPoolTask();
  EXPECT_TRUE(success);
  EXPECT_FALSE(cache_entry.is_dirty());
}

}  // namespace file_system
}  // namespace drive
