// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/remove_operation.h"

#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {

typedef OperationTestBase RemoveOperationTest;

TEST_F(RemoveOperationTest, RemoveFile) {
  RemoveOperation operation(blocking_task_runner(), observer(), scheduler(),
                            metadata(), cache());

  base::FilePath my_drive(FILE_PATH_LITERAL("drive/root"));
  base::FilePath nonexisting_file(
      FILE_PATH_LITERAL("drive/root/Dummy file.txt"));
  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath file_in_subdir(
      FILE_PATH_LITERAL("drive/root/Directory 1/SubDirectory File 1.txt"));

  // Remove a file in root.
  ResourceEntry entry;
  FileError error = FILE_ERROR_FAILED;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &entry));
  operation.Remove(file_in_root,
                   false,  // is_recursive
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(file_in_root, &entry));

  // Remove a file in subdirectory.
  error = FILE_ERROR_FAILED;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_subdir, &entry));
  const std::string resource_id = entry.resource_id();

  operation.Remove(file_in_subdir,
                   false,  // is_recursive
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(file_in_subdir, &entry));

  // Verify the file is indeed removed in the server.
  google_apis::GDataErrorCode gdata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> gdata_entry;
  fake_service()->GetResourceEntry(
      resource_id,
      google_apis::test_util::CreateCopyResultCallback(&gdata_error,
                                                       &gdata_entry));
  test_util::RunBlockingPoolTask();
  ASSERT_EQ(google_apis::HTTP_SUCCESS, gdata_error);
  EXPECT_TRUE(gdata_entry->deleted());

  // Try removing non-existing file.
  error = FILE_ERROR_FAILED;
  ASSERT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(nonexisting_file, &entry));
  operation.Remove(base::FilePath::FromUTF8Unsafe("drive/root/Dummy file.txt"),
                   false,  // is_recursive
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);

  // Verify observer notifications.
  EXPECT_EQ(2U, observer()->get_changed_paths().size());
  EXPECT_TRUE(observer()->get_changed_paths().count(file_in_root.DirName()));
  EXPECT_TRUE(observer()->get_changed_paths().count(file_in_subdir.DirName()));
}

TEST_F(RemoveOperationTest, RemoveDirectory) {
  RemoveOperation operation(blocking_task_runner(), observer(), scheduler(),
                            metadata(), cache());

  base::FilePath empty_dir(FILE_PATH_LITERAL(
      "drive/root/Directory 1/Sub Directory Folder/Sub Sub Directory Folder"));
  base::FilePath non_empty_dir(FILE_PATH_LITERAL(
      "drive/root/Directory 1"));
  base::FilePath file_in_non_empty_dir(FILE_PATH_LITERAL(
      "drive/root/Directory 1/SubDirectory File 1.txt"));

  // Empty directory can be removed even with is_recursive = false.
  FileError error = FILE_ERROR_FAILED;
  ResourceEntry entry;
  operation.Remove(empty_dir,
                   false,  // is_recursive
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(empty_dir, &entry));

  // Non-empty directory, cannot.
  error = FILE_ERROR_FAILED;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(non_empty_dir, &entry));
  operation.Remove(non_empty_dir,
                   false,  // is_recursive
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_EMPTY, error);
  EXPECT_EQ(FILE_ERROR_OK,
            GetLocalResourceEntry(non_empty_dir, &entry));

  // With is_recursive = true, it can be deleted, however. Descendant entries
  // are removed together.
  error = FILE_ERROR_FAILED;
  ASSERT_EQ(FILE_ERROR_OK,
            GetLocalResourceEntry(file_in_non_empty_dir, &entry));
  operation.Remove(non_empty_dir,
                   true,  // is_recursive
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(non_empty_dir, &entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(file_in_non_empty_dir, &entry));
}

TEST_F(RemoveOperationTest, RemoveShared) {
  RemoveOperation operation(blocking_task_runner(), observer(), scheduler(),
                            metadata(), cache());

  const base::FilePath kPathInMyDrive(FILE_PATH_LITERAL(
      "drive/root/shared.txt"));
  const base::FilePath kPathInOther(FILE_PATH_LITERAL(
      "drive/other/shared.txt"));

  // Prepare a shared file to the root folder.
  google_apis::GDataErrorCode gdata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> gdata_entry;
  fake_service()->AddNewFile(
      "text/plain",
      "dummy content",
      fake_service()->GetRootResourceId(),
      kPathInMyDrive.BaseName().AsUTF8Unsafe(),
      true,  // shared_with_me,
      google_apis::test_util::CreateCopyResultCallback(&gdata_error,
                                                       &gdata_entry));
  test_util::RunBlockingPoolTask();
  ASSERT_EQ(google_apis::HTTP_CREATED, gdata_error);
  CheckForUpdates();

  // Remove it. Locally, the file should be moved to drive/other.
  FileError error = FILE_ERROR_FAILED;
  ResourceEntry entry;
  operation.Remove(kPathInMyDrive,
                   false,  // is_recursive
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(kPathInMyDrive, &entry));
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(kPathInOther, &entry));

  // Remotely, the entry should have lost its parent.
  gdata_error = google_apis::GDATA_OTHER_ERROR;
  const std::string resource_id = gdata_entry->resource_id();
  fake_service()->GetResourceEntry(
      resource_id,
      google_apis::test_util::CreateCopyResultCallback(&gdata_error,
                                                       &gdata_entry));
  test_util::RunBlockingPoolTask();
  ASSERT_EQ(google_apis::HTTP_SUCCESS, gdata_error);
  EXPECT_FALSE(gdata_entry->deleted());  // It's not deleted.
  EXPECT_FALSE(gdata_entry->GetLinkByType(google_apis::Link::LINK_PARENT));
}

}  // namespace file_system
}  // namespace drive
