// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/open_file_operation.h"

#include <map>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {

class OpenFileOperationTest : public OperationTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
    OperationTestBase::SetUp();

    operation_.reset(new OpenFileOperation(
        blocking_task_runner(), observer(), scheduler(), metadata(), cache(),
        temp_dir()));
  }

  scoped_ptr<OpenFileOperation> operation_;
};

TEST_F(OpenFileOperationTest, OpenExistingFile) {
  const base::FilePath file_in_root(
      FILE_PATH_LITERAL("drive/root/File 1.txt"));
  ResourceEntry src_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &src_entry));
  const int64 file_size = src_entry.file_info().size();

  FileError error = FILE_ERROR_FAILED;
  base::FilePath file_path;
  base::Closure close_callback;
  operation_->OpenFile(
      file_in_root,
      OPEN_FILE,
      std::string(),  // mime_type
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &close_callback));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(base::PathExists(file_path));
  int64 local_file_size;
  ASSERT_TRUE(file_util::GetFileSize(file_path, &local_file_size));
  EXPECT_EQ(file_size, local_file_size);

  ASSERT_FALSE(close_callback.is_null());
  close_callback.Run();
  EXPECT_EQ(
      1U,
      observer()->upload_needed_local_ids().count(src_entry.resource_id()));
}

TEST_F(OpenFileOperationTest, OpenNonExistingFile) {
  const base::FilePath file_in_root(
      FILE_PATH_LITERAL("drive/root/not-exist.txt"));

  FileError error = FILE_ERROR_FAILED;
  base::FilePath file_path;
  base::Closure close_callback;
  operation_->OpenFile(
      file_in_root,
      OPEN_FILE,
      std::string(),  // mime_type
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &close_callback));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_TRUE(close_callback.is_null());
}

TEST_F(OpenFileOperationTest, CreateExistingFile) {
  const base::FilePath file_in_root(
      FILE_PATH_LITERAL("drive/root/File 1.txt"));
  ResourceEntry src_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &src_entry));

  FileError error = FILE_ERROR_FAILED;
  base::FilePath file_path;
  base::Closure close_callback;
  operation_->OpenFile(
      file_in_root,
      CREATE_FILE,
      std::string(),  // mime_type
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &close_callback));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_EXISTS, error);
  EXPECT_TRUE(close_callback.is_null());
}

TEST_F(OpenFileOperationTest, CreateNonExistingFile) {
  const base::FilePath file_in_root(
      FILE_PATH_LITERAL("drive/root/not-exist.txt"));

  FileError error = FILE_ERROR_FAILED;
  base::FilePath file_path;
  base::Closure close_callback;
  operation_->OpenFile(
      file_in_root,
      CREATE_FILE,
      std::string(),  // mime_type
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &close_callback));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(base::PathExists(file_path));
  int64 local_file_size;
  ASSERT_TRUE(file_util::GetFileSize(file_path, &local_file_size));
  EXPECT_EQ(0, local_file_size);  // Should be an empty file.

  ASSERT_FALSE(close_callback.is_null());
  close_callback.Run();
  // Here we don't know about the resource id, so just make sure
  // OnCacheFileUploadNeededByOperation is called actually.
  EXPECT_EQ(1U, observer()->upload_needed_local_ids().size());
}

TEST_F(OpenFileOperationTest, OpenOrCreateExistingFile) {
  const base::FilePath file_in_root(
      FILE_PATH_LITERAL("drive/root/File 1.txt"));
  ResourceEntry src_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &src_entry));
  const int64 file_size = src_entry.file_info().size();

  FileError error = FILE_ERROR_FAILED;
  base::FilePath file_path;
  base::Closure close_callback;
  operation_->OpenFile(
      file_in_root,
      OPEN_OR_CREATE_FILE,
      std::string(),  // mime_type
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &close_callback));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(base::PathExists(file_path));
  int64 local_file_size;
  ASSERT_TRUE(file_util::GetFileSize(file_path, &local_file_size));
  EXPECT_EQ(file_size, local_file_size);

  ASSERT_FALSE(close_callback.is_null());
  close_callback.Run();
  EXPECT_EQ(
      1U,
      observer()->upload_needed_local_ids().count(src_entry.resource_id()));
}

TEST_F(OpenFileOperationTest, OpenOrCreateNonExistingFile) {
  const base::FilePath file_in_root(
      FILE_PATH_LITERAL("drive/root/not-exist.txt"));

  FileError error = FILE_ERROR_FAILED;
  base::FilePath file_path;
  base::Closure close_callback;
  operation_->OpenFile(
      file_in_root,
      OPEN_OR_CREATE_FILE,
      std::string(),  // mime_type
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &close_callback));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(base::PathExists(file_path));
  int64 local_file_size;
  ASSERT_TRUE(file_util::GetFileSize(file_path, &local_file_size));
  EXPECT_EQ(0, local_file_size);  // Should be an empty file.

  ASSERT_FALSE(close_callback.is_null());
  close_callback.Run();
  // Here we don't know about the resource id, so just make sure
  // OnCacheFileUploadNeededByOperation is called actually.
  EXPECT_EQ(1U, observer()->upload_needed_local_ids().size());
}

TEST_F(OpenFileOperationTest, OpenFileTwice) {
  const base::FilePath file_in_root(
      FILE_PATH_LITERAL("drive/root/File 1.txt"));
  ResourceEntry src_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &src_entry));
  const int64 file_size = src_entry.file_info().size();

  FileError error = FILE_ERROR_FAILED;
  base::FilePath file_path;
  base::Closure close_callback;
  operation_->OpenFile(
      file_in_root,
      OPEN_FILE,
      std::string(),  // mime_type
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &close_callback));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(base::PathExists(file_path));
  int64 local_file_size;
  ASSERT_TRUE(file_util::GetFileSize(file_path, &local_file_size));
  EXPECT_EQ(file_size, local_file_size);

  // Open again.
  error = FILE_ERROR_FAILED;
  base::Closure close_callback2;
  operation_->OpenFile(
      file_in_root,
      OPEN_FILE,
      std::string(),  // mime_type
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &close_callback2));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(base::PathExists(file_path));
  ASSERT_TRUE(file_util::GetFileSize(file_path, &local_file_size));
  EXPECT_EQ(file_size, local_file_size);

  ASSERT_FALSE(close_callback.is_null());
  ASSERT_FALSE(close_callback2.is_null());

  close_callback.Run();

  // There still remains a client opening the file, so it shouldn't be
  // uploaded yet.
  EXPECT_TRUE(observer()->upload_needed_local_ids().empty());

  close_callback2.Run();

  // Here, all the clients close the file, so it should be uploaded then.
  EXPECT_EQ(
      1U,
      observer()->upload_needed_local_ids().count(src_entry.resource_id()));
}

}  // namespace file_system
}  // namespace drive
