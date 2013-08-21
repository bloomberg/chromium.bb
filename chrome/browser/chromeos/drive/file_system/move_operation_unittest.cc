// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/move_operation.h"

#include "chrome/browser/chromeos/drive/file_system/copy_operation.h"
#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {

class MoveOperationTest : public OperationTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
   OperationTestBase::SetUp();
   operation_.reset(new MoveOperation(blocking_task_runner(),
                                      observer(),
                                      scheduler(),
                                      metadata()));
   copy_operation_.reset(new CopyOperation(blocking_task_runner(),
                                           observer(),
                                           scheduler(),
                                           metadata(),
                                           cache(),
                                           fake_service(),
                                           temp_dir()));
 }

  scoped_ptr<MoveOperation> operation_;
  scoped_ptr<CopyOperation> copy_operation_;
};

TEST_F(MoveOperationTest, MoveFileInSameDirectory) {
  const base::FilePath src_path(
      FILE_PATH_LITERAL("drive/root/Directory 1/SubDirectory File 1.txt"));
  const base::FilePath dest_path(
      FILE_PATH_LITERAL("drive/root/Directory 1/Test.log"));

  ResourceEntry src_entry, dest_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &src_entry));
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(dest_path, &dest_entry));

  FileError error = FILE_ERROR_FAILED;
  operation_->Move(src_path,
                   dest_path,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &dest_entry));
  EXPECT_EQ(src_entry.resource_id(), dest_entry.resource_id());
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(src_path, &src_entry));

  EXPECT_EQ(1U, observer()->get_changed_paths().size());
  EXPECT_TRUE(observer()->get_changed_paths().count(src_path.DirName()));
}

TEST_F(MoveOperationTest, MoveFileFromRootToSubDirectory) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_path(
      FILE_PATH_LITERAL("drive/root/Directory 1/Test.log"));

  ResourceEntry src_entry, dest_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &src_entry));
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(dest_path, &dest_entry));

  FileError error = FILE_ERROR_FAILED;
  operation_->Move(src_path,
                   dest_path,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &dest_entry));
  EXPECT_EQ(src_entry.resource_id(), dest_entry.resource_id());
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(src_path, &src_entry));

  EXPECT_EQ(2U, observer()->get_changed_paths().size());
  EXPECT_TRUE(observer()->get_changed_paths().count(src_path.DirName()));
  EXPECT_TRUE(observer()->get_changed_paths().count(dest_path.DirName()));
}

TEST_F(MoveOperationTest, MoveFileFromSubDirectoryToRoot) {
  base::FilePath src_path(
      FILE_PATH_LITERAL("drive/root/Directory 1/SubDirectory File 1.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/Test.log"));

  ResourceEntry src_entry, dest_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &src_entry));
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(dest_path, &dest_entry));

  FileError error = FILE_ERROR_FAILED;
  operation_->Move(src_path,
                   dest_path,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &dest_entry));
  EXPECT_EQ(src_entry.resource_id(), dest_entry.resource_id());
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(src_path, &src_entry));

  EXPECT_EQ(2U, observer()->get_changed_paths().size());
  EXPECT_TRUE(observer()->get_changed_paths().count(src_path.DirName()));
  EXPECT_TRUE(observer()->get_changed_paths().count(dest_path.DirName()));
}

TEST_F(MoveOperationTest, MoveFileBetweenSubDirectories) {
  base::FilePath src_path(
      FILE_PATH_LITERAL("drive/root/Directory 1/SubDirectory File 1.txt"));
  base::FilePath dest_path(
      FILE_PATH_LITERAL("drive/root/Directory 1/Sub Directory Folder/Test"));

  ResourceEntry src_entry, dest_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &src_entry));
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(dest_path, &dest_entry));

  FileError error = FILE_ERROR_FAILED;
  operation_->Move(src_path,
                   dest_path,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &dest_entry));
  EXPECT_EQ(src_entry.resource_id(), dest_entry.resource_id());
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(src_path, &src_entry));

  EXPECT_EQ(2U, observer()->get_changed_paths().size());
  EXPECT_TRUE(observer()->get_changed_paths().count(src_path.DirName()));
  EXPECT_TRUE(observer()->get_changed_paths().count(dest_path.DirName()));
}

TEST_F(MoveOperationTest, MoveFileBetweenSubDirectoriesNoRename) {
  base::FilePath src_path(
      FILE_PATH_LITERAL("drive/root/Directory 1/SubDirectory File 1.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL(
      "drive/root/Directory 1/Sub Directory Folder/SubDirectory File 1.txt"));

  ResourceEntry src_entry, dest_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &src_entry));
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(dest_path, &dest_entry));

  FileError error = FILE_ERROR_FAILED;
  operation_->Move(src_path,
                   dest_path,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &dest_entry));
  EXPECT_EQ(src_entry.resource_id(), dest_entry.resource_id());
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(src_path, &src_entry));

  EXPECT_EQ(2U, observer()->get_changed_paths().size());
  EXPECT_TRUE(observer()->get_changed_paths().count(src_path.DirName()));
  EXPECT_TRUE(observer()->get_changed_paths().count(dest_path.DirName()));
}

TEST_F(MoveOperationTest, MoveFileBetweenSubDirectoriesRenameWithTitle) {
  base::FilePath src_path(
      FILE_PATH_LITERAL("drive/root/Directory 1/SubDirectory File 1.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL(
      "drive/root/Directory 1/Sub Directory Folder/"
      "SubDirectory File 1 (1).txt"));

  ResourceEntry src_entry, dest_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &src_entry));
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(dest_path, &dest_entry));

  FileError error = FILE_ERROR_FAILED;
  // Copy the src file into the same directory. This will make inconsistency
  // between title and path of the copied file.
  copy_operation_->Copy(
      src_path,
      src_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  base::FilePath copied_path(
      FILE_PATH_LITERAL("drive/root/Directory 1/SubDirectory File 1 (1).txt"));
  ResourceEntry copied_entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(copied_path, &copied_entry));
  ASSERT_EQ("SubDirectory File 1.txt", copied_entry.title());

  // Move the copied file.
  operation_->Move(copied_path,
                   dest_path,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &dest_entry));
  EXPECT_EQ("SubDirectory File 1 (1).txt", dest_entry.title());
  EXPECT_EQ(copied_entry.resource_id(), dest_entry.resource_id());
  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(copied_path, &copied_entry));

  EXPECT_EQ(2U, observer()->get_changed_paths().size());
  EXPECT_TRUE(observer()->get_changed_paths().count(copied_path.DirName()));
  EXPECT_TRUE(observer()->get_changed_paths().count(dest_path.DirName()));
}

TEST_F(MoveOperationTest, MoveNotExistingFile) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/Dummy file.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/Test.log"));

  FileError error = FILE_ERROR_OK;
  operation_->Move(src_path,
                   dest_path,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);

  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(dest_path, &entry));
}

TEST_F(MoveOperationTest, MoveFileToNonExistingDirectory) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_path(FILE_PATH_LITERAL("drive/root/Dummy/Test.log"));

  FileError error = FILE_ERROR_OK;
  operation_->Move(src_path,
                   dest_path,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);

  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(dest_path, &entry));
}

// Test the case where the parent of |dest_file_path| is a existing file,
// not a directory.
TEST_F(MoveOperationTest, MoveFileToInvalidPath) {
  base::FilePath src_path(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath dest_path(
      FILE_PATH_LITERAL("drive/root/Duplicate Name.txt/Test.log"));

  FileError error = FILE_ERROR_OK;
  operation_->Move(src_path,
                   dest_path,
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_A_DIRECTORY, error);

  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(dest_path, &entry));
}

}  // namespace file_system
}  // namespace drive
