// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/close_file_operation.h"

#include <set>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {

class CloseFileOperationTest : public OperationTestBase {
 protected:
  virtual void SetUp() {
    OperationTestBase::SetUp();

    operation_.reset(new CloseFileOperation(
        blocking_task_runner(), observer(), metadata(), &open_files_));
  }

  std::map<base::FilePath, int> open_files_;
  scoped_ptr<CloseFileOperation> operation_;
};

TEST_F(CloseFileOperationTest, CloseFile) {
  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  ResourceEntry src_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &src_entry));

  open_files_[file_in_root] = 1;
  FileError error = FILE_ERROR_FAILED;
  operation_->CloseFile(
      file_in_root,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_TRUE(open_files_.empty());
  EXPECT_EQ(
      1U,
      observer()->upload_needed_resource_ids().count(src_entry.resource_id()));
}

TEST_F(CloseFileOperationTest, NotOpenedFile) {
  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  ResourceEntry src_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &src_entry));

  FileError error = FILE_ERROR_FAILED;
  operation_->CloseFile(
      file_in_root,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  // Even if the file is actually exists, NOT_FOUND should be returned if the
  // file is not opened.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
}

TEST_F(CloseFileOperationTest, CloseFileTwice) {
  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  ResourceEntry src_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &src_entry));

  // Set the number of opening clients is two.
  open_files_[file_in_root] = 2;

  // The first CloseFile.
  FileError error = FILE_ERROR_FAILED;
  operation_->CloseFile(
      file_in_root,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(1, open_files_[file_in_root]);

  // There still remains a client opening the file, so it shouldn't be
  // uploaded yet.
  EXPECT_TRUE(observer()->upload_needed_resource_ids().empty());

  // The second CloseFile.
  operation_->CloseFile(
      file_in_root,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_TRUE(open_files_.empty());
  // Here, all the clients close the file, so it should be uploaded then.
  EXPECT_EQ(
      1U,
      observer()->upload_needed_resource_ids().count(src_entry.resource_id()));
}

}  // namespace file_system
}  // namespace drive
