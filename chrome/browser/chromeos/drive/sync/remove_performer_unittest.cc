// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/sync/remove_performer.h"

#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"

namespace drive {
namespace internal {

typedef file_system::OperationTestBase RemovePerformerTest;

TEST_F(RemovePerformerTest, RemoveFile) {
  RemovePerformer performer(blocking_task_runner(), scheduler(), metadata());

  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath file_in_subdir(
      FILE_PATH_LITERAL("drive/root/Directory 1/SubDirectory File 1.txt"));

  // Remove a file in root.
  ResourceEntry entry;
  FileError error = FILE_ERROR_FAILED;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &entry));
  performer.Remove(entry.local_id(),
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntry(file_in_root, &entry));

  // Remove a file in subdirectory.
  error = FILE_ERROR_FAILED;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_subdir, &entry));
  const std::string resource_id = entry.resource_id();

  performer.Remove(entry.local_id(),
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
  performer.Remove("non-existing-id",
                   google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
}

TEST_F(RemovePerformerTest, RemoveShared) {
  RemovePerformer performer(blocking_task_runner(), scheduler(), metadata());

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
  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(kPathInMyDrive, &entry));
  FileError error = FILE_ERROR_FAILED;
  performer.Remove(entry.local_id(),
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

}  // namespace internal
}  // namespace drive
