// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/copy_operation.h"

#include "base/file_util.h"
#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {

class CopyOperationTest : public OperationTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
   OperationTestBase::SetUp();
   operation_.reset(new CopyOperation(blocking_task_runner(),
                                      observer(),
                                      scheduler(),
                                      metadata(),
                                      cache(),
                                      fake_service(),
                                      temp_dir()));
  }

  scoped_ptr<CopyOperation> operation_;
};

TEST_F(CopyOperationTest, TransferFileFromLocalToRemote_RegularFile) {
  const base::FilePath local_src_path = temp_dir().AppendASCII("local.txt");
  const base::FilePath remote_dest_path(
      FILE_PATH_LITERAL("drive/root/remote.txt"));

  // Prepare a local file.
  ASSERT_TRUE(
      google_apis::test_util::WriteStringToFile(local_src_path, "hello"));
  // Confirm that the remote file does not exist.
  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(remote_dest_path, &entry));

  // Transfer the local file to Drive.
  FileError error = FILE_ERROR_FAILED;
  operation_->TransferFileFromLocalToRemote(
      local_src_path,
      remote_dest_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // TransferFileFromLocalToRemote stores a copy of the local file in the cache,
  // marks it dirty and requests the observer to upload the file.
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_dest_path, &entry));
  EXPECT_EQ(1U, observer()->upload_needed_local_ids().count(
      GetLocalId(remote_dest_path)));
  FileCacheEntry cache_entry;
  bool found = false;
  cache()->GetCacheEntryOnUIThread(
      GetLocalId(remote_dest_path),
      google_apis::test_util::CreateCopyResultCallback(&found, &cache_entry));
  test_util::RunBlockingPoolTask();
  EXPECT_TRUE(found);
  EXPECT_TRUE(cache_entry.is_present());
  EXPECT_TRUE(cache_entry.is_dirty());

  EXPECT_EQ(1U, observer()->get_changed_paths().size());
  EXPECT_TRUE(observer()->get_changed_paths().count(
      remote_dest_path.DirName()));
}

TEST_F(CopyOperationTest, TransferFileFromLocalToRemote_QuotaCheck) {
  const base::FilePath local_src_path = temp_dir().AppendASCII("local.txt");
  const base::FilePath remote_dest_path(
      FILE_PATH_LITERAL("drive/root/remote.txt"));

  const size_t kFileSize = 10;

  // Prepare a local file.
  ASSERT_TRUE(
      google_apis::test_util::WriteStringToFile(local_src_path,
                                                std::string(kFileSize, 'a')));

  // Set insufficient quota.
  fake_service()->SetQuotaValue(100, 100 + kFileSize - 1);

  // Transfer the local file to Drive.
  FileError error = FILE_ERROR_FAILED;
  operation_->TransferFileFromLocalToRemote(
      local_src_path,
      remote_dest_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NO_SERVER_SPACE, error);

  // Set sufficient quota.
  fake_service()->SetQuotaValue(100, 100 + kFileSize);

  // Transfer the local file to Drive.
  error = FILE_ERROR_FAILED;
  operation_->TransferFileFromLocalToRemote(
      local_src_path,
      remote_dest_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
}

TEST_F(CopyOperationTest, TransferFileFromLocalToRemote_HostedDocument) {
  const base::FilePath local_src_path = temp_dir().AppendASCII("local.gdoc");
  const base::FilePath remote_dest_path(FILE_PATH_LITERAL(
      "drive/root/Directory 1/Document 1 excludeDir-test.gdoc"));

  // Prepare a local file, which is a json file of a hosted document, which
  // matches "Document 1" in root_feed.json.
  ASSERT_TRUE(util::CreateGDocFile(
      local_src_path,
      GURL("https://3_document_self_link/document:5_document_resource_id"),
      "document:5_document_resource_id"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(remote_dest_path, &entry));

  // Transfer the local file to Drive.
  FileError error = FILE_ERROR_FAILED;
  operation_->TransferFileFromLocalToRemote(
      local_src_path,
      remote_dest_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(remote_dest_path, &entry));

  // We added a file to the Drive root and then moved to "Directory 1".
  if (util::IsDriveV2ApiEnabled()) {
    EXPECT_EQ(1U, observer()->get_changed_paths().size());
    EXPECT_TRUE(
        observer()->get_changed_paths().count(remote_dest_path.DirName()));
  } else {
    EXPECT_EQ(2U, observer()->get_changed_paths().size());
    EXPECT_TRUE(observer()->get_changed_paths().count(
        base::FilePath(FILE_PATH_LITERAL("drive/root"))));
    EXPECT_TRUE(observer()->get_changed_paths().count(
        remote_dest_path.DirName()));
  }
}


TEST_F(CopyOperationTest, CopyNotExistingFile) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/Dummy file.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/Test.log"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(src_path, &entry));

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);

  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(dest_path, &entry));
  EXPECT_TRUE(observer()->get_changed_paths().empty());
}

TEST_F(CopyOperationTest, CopyFileToNonExistingDirectory) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/Dummy/Test.log"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(dest_path.DirName(), &entry));

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(dest_path, &entry));
  EXPECT_TRUE(observer()->get_changed_paths().empty());
}

// Test the case where the parent of the destination path is an existing file,
// not a directory.
TEST_F(CopyOperationTest, CopyFileToInvalidPath) {
  base::FilePath src_path(FILE_PATH_LITERAL(
      "drive/root/Document 1 excludeDir-test.gdoc"));
  base::FilePath dest_path(FILE_PATH_LITERAL(
      "drive/root/Duplicate Name.txt/Document 1 excludeDir-test.gdoc"));

  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path.DirName(), &entry));
  ASSERT_FALSE(entry.file_info().is_directory());

  FileError error = FILE_ERROR_OK;
  operation_->Copy(src_path,
                   dest_path,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_A_DIRECTORY, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(dest_path, &entry));
  EXPECT_TRUE(observer()->get_changed_paths().empty());
}

}  // namespace file_system
}  // namespace drive
