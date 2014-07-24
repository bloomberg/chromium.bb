// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/sync/remove_performer.h"

#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "content/public/test/test_utils.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/test_util.h"

namespace drive {
namespace internal {

typedef file_system::OperationTestBase RemovePerformerTest;

TEST_F(RemovePerformerTest, RemoveFile) {
  RemovePerformer performer(blocking_task_runner(), delegate(), scheduler(),
                            metadata());

  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/root/File 1.txt"));
  base::FilePath file_in_subdir(
      FILE_PATH_LITERAL("drive/root/Directory 1/SubDirectory File 1.txt"));

  // Remove a file in root.
  ResourceEntry entry;
  FileError error = FILE_ERROR_FAILED;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_root, &entry));
  performer.Remove(entry.local_id(),
                   ClientContext(USER_INITIATED),
                   google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Remove a file in subdirectory.
  error = FILE_ERROR_FAILED;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(file_in_subdir, &entry));
  const std::string resource_id = entry.resource_id();

  performer.Remove(entry.local_id(),
                   ClientContext(USER_INITIATED),
                   google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Verify the file is indeed removed in the server.
  google_apis::GDataErrorCode gdata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::FileResource> gdata_entry;
  fake_service()->GetFileResource(
      resource_id,
      google_apis::test_util::CreateCopyResultCallback(&gdata_error,
                                                       &gdata_entry));
  content::RunAllBlockingPoolTasksUntilIdle();
  ASSERT_EQ(google_apis::HTTP_SUCCESS, gdata_error);
  EXPECT_TRUE(gdata_entry->labels().is_trashed());

  // Try removing non-existing file.
  error = FILE_ERROR_FAILED;
  performer.Remove("non-existing-id",
                   ClientContext(USER_INITIATED),
                   google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
}

TEST_F(RemovePerformerTest, RemoveShared) {
  RemovePerformer performer(blocking_task_runner(), delegate(), scheduler(),
                            metadata());

  const base::FilePath kPathInMyDrive(FILE_PATH_LITERAL(
      "drive/root/shared.txt"));
  const base::FilePath kPathInOther(FILE_PATH_LITERAL(
      "drive/other/shared.txt"));

  // Prepare a shared file to the root folder.
  google_apis::GDataErrorCode gdata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::FileResource> gdata_entry;
  fake_service()->AddNewFile(
      "text/plain",
      "dummy content",
      fake_service()->GetRootResourceId(),
      kPathInMyDrive.BaseName().AsUTF8Unsafe(),
      true,  // shared_with_me,
      google_apis::test_util::CreateCopyResultCallback(&gdata_error,
                                                       &gdata_entry));
  content::RunAllBlockingPoolTasksUntilIdle();
  ASSERT_EQ(google_apis::HTTP_CREATED, gdata_error);
  CheckForUpdates();

  // Remove it. Locally, the file should be moved to drive/other.
  ResourceEntry entry;
  ASSERT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(kPathInMyDrive, &entry));
  FileError error = FILE_ERROR_FAILED;
  performer.Remove(entry.local_id(),
                   ClientContext(USER_INITIATED),
                   google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            GetLocalResourceEntry(kPathInMyDrive, &entry));
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(kPathInOther, &entry));

  // Remotely, the entry should have lost its parent.
  gdata_error = google_apis::GDATA_OTHER_ERROR;
  const std::string resource_id = gdata_entry->file_id();
  fake_service()->GetFileResource(
      resource_id,
      google_apis::test_util::CreateCopyResultCallback(&gdata_error,
                                                       &gdata_entry));
  content::RunAllBlockingPoolTasksUntilIdle();
  ASSERT_EQ(google_apis::HTTP_SUCCESS, gdata_error);
  EXPECT_FALSE(gdata_entry->labels().is_trashed());  // It's not deleted.
  EXPECT_TRUE(gdata_entry->parents().empty());
}

TEST_F(RemovePerformerTest, RemoveLocallyCreatedFile) {
  RemovePerformer performer(blocking_task_runner(), delegate(), scheduler(),
                            metadata());

  // Add an entry without resource ID.
  ResourceEntry entry;
  entry.set_title("New File.txt");
  entry.set_parent_local_id(util::kDriveTrashDirLocalId);

  FileError error = FILE_ERROR_FAILED;
  std::string local_id;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&ResourceMetadata::AddEntry,
                 base::Unretained(metadata()),
                 entry,
                 &local_id),
      google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Remove the entry.
  performer.Remove(local_id, ClientContext(USER_INITIATED),
                   google_apis::test_util::CreateCopyResultCallback(&error));
  content::RunAllBlockingPoolTasksUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, GetLocalResourceEntryById(local_id, &entry));
}

}  // namespace internal
}  // namespace drive
