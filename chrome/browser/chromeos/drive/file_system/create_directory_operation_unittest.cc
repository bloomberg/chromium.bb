// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/create_directory_operation.h"

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/change_list_loader.h"
#include "chrome/browser/chromeos/drive/drive_webapps_registry.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {

class CreateDirectoryOperationTest
    : public testing::Test, public OperationObserver {
 protected:
  CreateDirectoryOperationTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());

    profile_.reset(new TestingProfile);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    fake_drive_service_.reset(new google_apis::FakeDriveService);
    fake_drive_service_->LoadResourceListForWapi(
        "chromeos/gdata/root_feed.json");
    fake_drive_service_->LoadAccountMetadataForWapi(
        "chromeos/gdata/account_metadata.json");
    fake_drive_service_->LoadAppListForDriveApi("chromeos/drive/applist.json");

    metadata_.reset(new internal::ResourceMetadata(temp_dir_.path(),
                                                   blocking_task_runner_));

    FileError error = FILE_ERROR_FAILED;
    metadata_->Initialize(
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    ASSERT_EQ(FILE_ERROR_OK, error);

    scheduler_.reset(
        new JobScheduler(profile_.get(), fake_drive_service_.get()));

    DriveWebAppsRegistry drive_web_apps_registry;
    ChangeListLoader change_list_loader(
        metadata_.get(), scheduler_.get(), &drive_web_apps_registry);

    // Makes sure the FakeDriveService's content is loaded to the metadata_.
    change_list_loader.LoadIfNeeded(
        DirectoryFetchInfo(),
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    ASSERT_EQ(FILE_ERROR_OK, error);

    operation_.reset(
        new CreateDirectoryOperation(scheduler_.get(), metadata_.get(), this));
  }

  virtual void TearDown() OVERRIDE {
    operation_.reset();
    scheduler_.reset();
    metadata_.reset();
    fake_drive_service_.reset();
    profile_.reset();

    blocking_task_runner_ = NULL;
  }

  virtual void OnDirectoryChangedByOperation(
      const base::FilePath& directory_path) OVERRIDE {
    // Do nothing.
  }

  CreateDirectoryOperation* operation() const {
    return operation_.get();
  }

 private:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  scoped_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;

  scoped_ptr<google_apis::FakeDriveService> fake_drive_service_;
  scoped_ptr<internal::ResourceMetadata, test_util::DestroyHelperForTests>
      metadata_;
  scoped_ptr<JobScheduler> scheduler_;

  scoped_ptr<CreateDirectoryOperation> operation_;
};

TEST_F(CreateDirectoryOperationTest, FindFirstMissingParentDirectory) {
  CreateDirectoryOperation::FindFirstMissingParentDirectoryResult result;

  // Create directory in root.
  base::FilePath dir_path(FILE_PATH_LITERAL("drive/root/New Folder 1"));
  operation()->FindFirstMissingParentDirectory(
      dir_path,
      google_apis::test_util::CreateCopyResultCallback(&result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(CreateDirectoryOperation::FIND_FIRST_FOUND_MISSING, result.error);
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("drive/root/New Folder 1")),
            result.first_missing_parent_path);
  EXPECT_EQ("fake_root", result.last_dir_resource_id);

  // Missing folders in subdir of an existing folder.
  base::FilePath dir_path2(
      FILE_PATH_LITERAL("drive/root/Directory 1/New Folder 2"));
  operation()->FindFirstMissingParentDirectory(
      dir_path2,
      google_apis::test_util::CreateCopyResultCallback(&result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(CreateDirectoryOperation::FIND_FIRST_FOUND_MISSING, result.error);
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL(
                "drive/root/Directory 1/New Folder 2")),
            result.first_missing_parent_path);
  EXPECT_NE("fake_root", result.last_dir_resource_id);  // non-root dir.

  // Missing two folders on the path.
  base::FilePath dir_path3 =
      dir_path2.Append(FILE_PATH_LITERAL("Another Folder"));
  operation()->FindFirstMissingParentDirectory(
      dir_path3,
      google_apis::test_util::CreateCopyResultCallback(&result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(CreateDirectoryOperation::FIND_FIRST_FOUND_MISSING, result.error);
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL(
                "drive/root/Directory 1/New Folder 2")),
            result.first_missing_parent_path);
  EXPECT_NE("fake_root", result.last_dir_resource_id);  // non-root dir.

  // Folders on top of an existing file.
  operation()->FindFirstMissingParentDirectory(
      base::FilePath(FILE_PATH_LITERAL("drive/root/File 1.txt/BadDir")),
      google_apis::test_util::CreateCopyResultCallback(&result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(CreateDirectoryOperation::FIND_FIRST_FOUND_INVALID, result.error);

  // Existing folder.
  operation()->FindFirstMissingParentDirectory(
      base::FilePath(FILE_PATH_LITERAL("drive/root/Directory 1")),
      google_apis::test_util::CreateCopyResultCallback(&result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(CreateDirectoryOperation::FIND_FIRST_DIRECTORY_ALREADY_PRESENT,
            result.error);
}

}  // namespace file_system
}  // namespace drive
