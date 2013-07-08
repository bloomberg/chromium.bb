// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/open_file_operation.h"

#include <set>

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
  virtual void SetUp() {
    OperationTestBase::SetUp();

    operation_.reset(new OpenFileOperation(
        blocking_task_runner(), observer(), scheduler(), metadata(), cache(),
        temp_dir(), &open_files_));
  }

  std::set<base::FilePath> open_files_;
  scoped_ptr<OpenFileOperation> operation_;
};

TEST_F(OpenFileOperationTest, OpenFile) {
  const base::FilePath file_in_root(
      FILE_PATH_LITERAL("drive/root/File 1.txt"));
  ResourceEntry src_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &src_entry));
  const int64 file_size = src_entry.file_info().size();

  FileError error = FILE_ERROR_FAILED;
  base::FilePath file_path;
  operation_->OpenFile(
      file_in_root,
      google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(file_util::PathExists(file_path));
  int64 local_file_size;
  ASSERT_TRUE(file_util::GetFileSize(file_path, &local_file_size));
  EXPECT_EQ(file_size, local_file_size);

  // The file_path should be added into the set.
  EXPECT_EQ(1U, open_files_.count(file_in_root));

  // Open the same file should be failed.
  error = FILE_ERROR_FAILED;
  file_path.clear();
  operation_->OpenFile(
      file_in_root,
      google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_IN_USE, error);
}

TEST_F(OpenFileOperationTest, NotFound) {
  const base::FilePath file_in_root(
      FILE_PATH_LITERAL("drive/root/not-exist.txt"));

  FileError error = FILE_ERROR_FAILED;
  base::FilePath file_path;
  operation_->OpenFile(
      file_in_root,
      google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);

  // The file shouldn't be in the set of opened files.
  EXPECT_EQ(0U, open_files_.count(file_in_root));
}

}  // namespace file_system
}  // namespace drive
