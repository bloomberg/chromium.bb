// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/download_operation.h"

#include "base/file_util.h"
#include "chrome/browser/chromeos/drive/fake_free_disk_space_getter.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {

class DownloadOperationTest : public OperationTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
    OperationTestBase::SetUp();

    operation_.reset(new DownloadOperation(
        blocking_task_runner(), observer(), scheduler(), metadata(), cache(),
        temp_dir()));
  }

  scoped_ptr<DownloadOperation> operation_;
};

TEST_F(DownloadOperationTest,
       EnsureFileDownloadedByPath_FromServer_EnoughSpace) {
  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  ResourceEntry src_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &src_entry));
  const int64 file_size = src_entry.file_info().size();

  // Pretend we have enough space.
  fake_free_disk_space_getter()->set_default_value(
      file_size + internal::kMinFreeSpace);

  FileError error = FILE_ERROR_FAILED;
  base::FilePath file_path;
  scoped_ptr<ResourceEntry> entry;
  operation_->EnsureFileDownloadedByPath(
      file_in_root,
      ClientContext(USER_INITIATED),
      GetFileContentInitializedCallback(),
      google_apis::GetContentCallback(),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &entry));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(entry);
  EXPECT_FALSE(entry->file_specific_info().is_hosted_document());

  // The transfered file is cached and the change of "offline available"
  // attribute is notified.
  EXPECT_EQ(1U, observer()->get_changed_paths().size());
  EXPECT_EQ(1U, observer()->get_changed_paths().count(file_in_root.DirName()));

  // Verify that readable permission is set.
  int permission = 0;
  EXPECT_TRUE(file_util::GetPosixFilePermissions(file_path, &permission));
  EXPECT_EQ(file_util::FILE_PERMISSION_READ_BY_USER |
            file_util::FILE_PERMISSION_WRITE_BY_USER |
            file_util::FILE_PERMISSION_READ_BY_GROUP |
            file_util::FILE_PERMISSION_READ_BY_OTHERS, permission);
}

TEST_F(DownloadOperationTest,
       EnsureFileDownloadedByPath_FromServer_NoSpaceAtAll) {
  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));

  // Pretend we have no space at all.
  fake_free_disk_space_getter()->set_default_value(0);

  FileError error = FILE_ERROR_OK;
  base::FilePath file_path;
  scoped_ptr<ResourceEntry> entry;
  operation_->EnsureFileDownloadedByPath(
      file_in_root,
      ClientContext(USER_INITIATED),
      GetFileContentInitializedCallback(),
      google_apis::GetContentCallback(),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &entry));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_NO_LOCAL_SPACE, error);
}

TEST_F(DownloadOperationTest,
       EnsureFileDownloadedByPath_FromServer_NoEnoughSpaceButCanFreeUp) {
  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  ResourceEntry src_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &src_entry));
  const int64 file_size = src_entry.file_info().size();

  // Pretend we have no space first (checked before downloading a file),
  // but then start reporting we have space. This is to emulate that
  // the disk space was freed up by removing temporary files.
  fake_free_disk_space_getter()->PushFakeValue(
      file_size + internal::kMinFreeSpace);
  fake_free_disk_space_getter()->PushFakeValue(0);
  fake_free_disk_space_getter()->set_default_value(
      file_size + internal::kMinFreeSpace);

  // Store something of the file size in the temporary cache directory.
  const std::string content(file_size, 'x');
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath tmp_file =
      temp_dir.path().AppendASCII("something.txt");
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(tmp_file, content));

  FileError error = FILE_ERROR_FAILED;
  cache()->StoreOnUIThread(
      "<resource_id>", "<md5>", tmp_file,
      internal::FileCache::FILE_OPERATION_COPY,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  base::FilePath file_path;
  scoped_ptr<ResourceEntry> entry;
  operation_->EnsureFileDownloadedByPath(
      file_in_root,
      ClientContext(USER_INITIATED),
      GetFileContentInitializedCallback(),
      google_apis::GetContentCallback(),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &entry));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(entry);
  EXPECT_FALSE(entry->file_specific_info().is_hosted_document());

  // The transfered file is cached and the change of "offline available"
  // attribute is notified.
  EXPECT_EQ(1U, observer()->get_changed_paths().size());
  EXPECT_EQ(1U, observer()->get_changed_paths().count(file_in_root.DirName()));

  // The cache entry should be removed in order to free up space.
  FileCacheEntry cache_entry;
  bool result = true;
  cache()->GetCacheEntryOnUIThread(
      "<resource_id>",
      google_apis::test_util::CreateCopyResultCallback(&result,
                                                       &cache_entry));
  test_util::RunBlockingPoolTask();
  ASSERT_FALSE(result);
}

TEST_F(DownloadOperationTest,
       EnsureFileDownloadedByPath_FromServer_EnoughSpaceButBecomeFull) {
  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  ResourceEntry src_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &src_entry));
  const int64 file_size = src_entry.file_info().size();

  // Pretend we have enough space first (checked before downloading a file),
  // but then start reporting we have not enough space. This is to emulate that
  // the disk space becomes full after the file is downloaded for some reason
  // (ex. the actual file was larger than the expected size).
  fake_free_disk_space_getter()->PushFakeValue(
      file_size + internal::kMinFreeSpace);
  fake_free_disk_space_getter()->set_default_value(
      internal::kMinFreeSpace - 1);

  FileError error = FILE_ERROR_OK;
  base::FilePath file_path;
  scoped_ptr<ResourceEntry> entry;
  operation_->EnsureFileDownloadedByPath(
      file_in_root,
      ClientContext(USER_INITIATED),
      GetFileContentInitializedCallback(),
      google_apis::GetContentCallback(),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &entry));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_NO_LOCAL_SPACE, error);
}

TEST_F(DownloadOperationTest, EnsureFileDownloadedByPath_FromCache) {
  base::FilePath temp_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir(), &temp_file));

  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  ResourceEntry src_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &src_entry));

  // Store something as cached version of this file.
  FileError error = FILE_ERROR_OK;
  cache()->StoreOnUIThread(
      src_entry.resource_id(),
      src_entry.file_specific_info().md5(),
      temp_file,
      internal::FileCache::FILE_OPERATION_COPY,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  base::FilePath file_path;
  scoped_ptr<ResourceEntry> entry;
  operation_->EnsureFileDownloadedByPath(
      file_in_root,
      ClientContext(USER_INITIATED),
      GetFileContentInitializedCallback(),
      google_apis::GetContentCallback(),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &entry));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(entry);
  EXPECT_FALSE(entry->file_specific_info().is_hosted_document());
}

TEST_F(DownloadOperationTest, EnsureFileDownloadedByPath_HostedDocument) {
  base::FilePath file_in_root(FILE_PATH_LITERAL(
      "drive/root/Document 1 excludeDir-test.gdoc"));

  FileError error = FILE_ERROR_FAILED;
  base::FilePath file_path;
  scoped_ptr<ResourceEntry> entry;
  operation_->EnsureFileDownloadedByPath(
      file_in_root,
      ClientContext(USER_INITIATED),
      GetFileContentInitializedCallback(),
      google_apis::GetContentCallback(),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &entry));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->file_specific_info().is_hosted_document());
  EXPECT_FALSE(file_path.empty());

  EXPECT_EQ(GURL(entry->file_specific_info().alternate_url()),
            util::ReadUrlFromGDocFile(file_path));
  EXPECT_EQ(entry->resource_id(), util::ReadResourceIdFromGDocFile(file_path));
}

TEST_F(DownloadOperationTest, EnsureFileDownloadedByResourceId) {
  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  ResourceEntry src_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &src_entry));
  std::string resource_id = src_entry.resource_id();

  FileError error = FILE_ERROR_OK;
  base::FilePath file_path;
  scoped_ptr<ResourceEntry> entry;
  operation_->EnsureFileDownloadedByResourceId(
      resource_id,
      ClientContext(USER_INITIATED),
      GetFileContentInitializedCallback(),
      google_apis::GetContentCallback(),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &entry));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(entry);
  EXPECT_FALSE(entry->file_specific_info().is_hosted_document());

  // The transfered file is cached and the change of "offline available"
  // attribute is notified.
  EXPECT_EQ(1U, observer()->get_changed_paths().size());
  EXPECT_EQ(1U, observer()->get_changed_paths().count(file_in_root.DirName()));
}

TEST_F(DownloadOperationTest,
       EnsureFileDownloadedByPath_WithGetContentCallback) {
  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));

  {
    FileError initialized_error = FILE_ERROR_FAILED;
    scoped_ptr<ResourceEntry> entry, entry_dontcare;
    base::FilePath local_path, local_path_dontcare;
    base::Closure cancel_download;
    google_apis::test_util::TestGetContentCallback get_content_callback;

    FileError completion_error = FILE_ERROR_FAILED;

    operation_->EnsureFileDownloadedByPath(
        file_in_root,
        ClientContext(USER_INITIATED),
        google_apis::test_util::CreateCopyResultCallback(
            &initialized_error, &entry, &local_path, &cancel_download),
        get_content_callback.callback(),
        google_apis::test_util::CreateCopyResultCallback(
            &completion_error, &local_path_dontcare, &entry_dontcare));
    test_util::RunBlockingPoolTask();

    // For the first time, file is downloaded from the remote server.
    // In this case, |local_path| is empty while |cancel_download| is not.
    EXPECT_EQ(FILE_ERROR_OK, initialized_error);
    ASSERT_TRUE(entry);
    ASSERT_TRUE(local_path.empty());
    EXPECT_TRUE(!cancel_download.is_null());
    // Content is available through the second callback argument.
    EXPECT_EQ(static_cast<size_t>(entry->file_info().size()),
              get_content_callback.GetConcatenatedData().size());
    EXPECT_EQ(FILE_ERROR_OK, completion_error);

    // The transfered file is cached and the change of "offline available"
    // attribute is notified.
    EXPECT_EQ(1U, observer()->get_changed_paths().size());
    EXPECT_EQ(1U,
              observer()->get_changed_paths().count(file_in_root.DirName()));
  }

  {
    FileError initialized_error = FILE_ERROR_FAILED;
    scoped_ptr<ResourceEntry> entry, entry_dontcare;
    base::FilePath local_path, local_path_dontcare;
    base::Closure cancel_download;
    google_apis::test_util::TestGetContentCallback get_content_callback;

    FileError completion_error = FILE_ERROR_FAILED;

    operation_->EnsureFileDownloadedByPath(
        file_in_root,
        ClientContext(USER_INITIATED),
        google_apis::test_util::CreateCopyResultCallback(
            &initialized_error, &entry, &local_path, &cancel_download),
        get_content_callback.callback(),
        google_apis::test_util::CreateCopyResultCallback(
            &completion_error, &local_path_dontcare, &entry_dontcare));
    test_util::RunBlockingPoolTask();

    // Try second download. In this case, the file should be cached, so
    // |local_path| should not be empty while |cancel_download| is empty.
    EXPECT_EQ(FILE_ERROR_OK, initialized_error);
    ASSERT_TRUE(entry);
    ASSERT_TRUE(!local_path.empty());
    EXPECT_TRUE(cancel_download.is_null());
    // The content is available from the cache file.
    EXPECT_TRUE(get_content_callback.data().empty());
    int64 local_file_size = 0;
    file_util::GetFileSize(local_path, &local_file_size);
    EXPECT_EQ(entry->file_info().size(), local_file_size);
    EXPECT_EQ(FILE_ERROR_OK, completion_error);
  }
}

TEST_F(DownloadOperationTest, EnsureFileDownloadedByResourceId_FromCache) {
  base::FilePath temp_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir(), &temp_file));

  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  ResourceEntry src_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &src_entry));

  // Store something as cached version of this file.
  FileError error = FILE_ERROR_FAILED;
  cache()->StoreOnUIThread(
      src_entry.resource_id(),
      src_entry.file_specific_info().md5(),
      temp_file,
      internal::FileCache::FILE_OPERATION_COPY,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // The file is obtained from the cache.
  // Hence the downloading should work even if the drive service is offline.
  fake_service()->set_offline(true);

  std::string resource_id = src_entry.resource_id();
  base::FilePath file_path;
  scoped_ptr<ResourceEntry> entry;
  operation_->EnsureFileDownloadedByResourceId(
      resource_id,
      ClientContext(USER_INITIATED),
      GetFileContentInitializedCallback(),
      google_apis::GetContentCallback(),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &entry));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(entry);
  EXPECT_FALSE(entry->file_specific_info().is_hosted_document());
}

TEST_F(DownloadOperationTest, EnsureFileDownloadedByPath_DirtyCache) {
  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  ResourceEntry src_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &src_entry));

  // Prepare a dirty file to store to cache that has a different size than
  // stored in resource metadata.
  base::FilePath dirty_file = temp_dir().AppendASCII("dirty.txt");
  size_t dirty_size = src_entry.file_info().size() + 10;
  google_apis::test_util::WriteStringToFile(dirty_file,
                                            std::string(dirty_size, 'x'));

  // Store the file as a cache, marking it to be dirty.
  FileError error = FILE_ERROR_FAILED;
  cache()->StoreOnUIThread(
      src_entry.resource_id(),
      src_entry.file_specific_info().md5(),
      dirty_file,
      internal::FileCache::FILE_OPERATION_COPY,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  cache()->MarkDirtyOnUIThread(
      src_entry.resource_id(),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Record values passed to GetFileContentInitializedCallback().
  FileError init_error;
  base::FilePath init_path;
  scoped_ptr<ResourceEntry> init_entry;
  base::Closure cancel_callback;

  base::FilePath file_path;
  scoped_ptr<ResourceEntry> entry;
  operation_->EnsureFileDownloadedByPath(
      file_in_root,
      ClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(
          &init_error, &init_entry, &init_path, &cancel_callback),
      google_apis::GetContentCallback(),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &entry));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  // Check that the result of local modification is propagated.
  EXPECT_EQ(static_cast<int64>(dirty_size), init_entry->file_info().size());
  EXPECT_EQ(static_cast<int64>(dirty_size), entry->file_info().size());
}

}  // namespace file_system
}  // namespace drive
