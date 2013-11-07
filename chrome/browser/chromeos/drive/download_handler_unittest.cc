// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/download_handler.h"

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/drive/dummy_file_system.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

namespace {

// Flags to control the state of the test file system.
enum DownloadPathState {
  // Simulates the state that requested path just simply exists.
  PATH_EXISTS,
  // Simulates the state that requested path fails to be accessed.
  PATH_INVALID,
  // Simulates the state that the requested path does not exist.
  PATH_NOT_EXIST,
  // Simulates the state that the path does not exist nor be able to be created.
  PATH_NOT_EXIST_AND_CREATE_FAIL,
};

// Test file system for verifying the behavior of DownloadHandler, by simulating
// various responses from FileSystem.
class DownloadHandlerTestFileSystem : public DummyFileSystem {
 public:
  DownloadHandlerTestFileSystem() : state_(PATH_INVALID) {}

  void set_download_path_state(DownloadPathState state) { state_ = state; }

  // FileSystemInterface overrides.
  virtual void GetResourceEntry(
      const base::FilePath& file_path,
      const GetResourceEntryCallback& callback) OVERRIDE {
    if (state_ == PATH_EXISTS) {
      callback.Run(FILE_ERROR_OK, make_scoped_ptr(new ResourceEntry));
      return;
    }
    callback.Run(
        state_ == PATH_INVALID ? FILE_ERROR_FAILED : FILE_ERROR_NOT_FOUND,
        scoped_ptr<ResourceEntry>());
 }

 virtual void CreateDirectory(
     const base::FilePath& directory_path,
     bool is_exclusive,
     bool is_recursive,
     const FileOperationCallback& callback) OVERRIDE {
   callback.Run(state_ == PATH_NOT_EXIST ? FILE_ERROR_OK : FILE_ERROR_FAILED);
 }

 private:
  DownloadPathState state_;
};

}  // namespace

class DownloadHandlerTest : public testing::Test {
 public:
  DownloadHandlerTest()
      : download_manager_(new content::MockDownloadManager) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Set expectations for download item.
    EXPECT_CALL(download_item_, GetState())
        .WillRepeatedly(testing::Return(content::DownloadItem::IN_PROGRESS));

    download_handler_.reset(new DownloadHandler(&test_file_system_));
    download_handler_->Initialize(download_manager_.get(), temp_dir_.path());
  }

 protected:
  base::ScopedTempDir temp_dir_;
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<content::MockDownloadManager> download_manager_;
  DownloadHandlerTestFileSystem test_file_system_;
  scoped_ptr<DownloadHandler> download_handler_;
  content::MockDownloadItem download_item_;
};

TEST_F(DownloadHandlerTest, SubstituteDriveDownloadPathNonDrivePath) {
  const base::FilePath non_drive_path(FILE_PATH_LITERAL("/foo/bar"));
  ASSERT_FALSE(util::IsUnderDriveMountPoint(non_drive_path));

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      non_drive_path,
      &download_item_,
      google_apis::test_util::CreateCopyResultCallback(&substituted_path));
  test_util::RunBlockingPoolTask();

  // Check the result.
  EXPECT_EQ(non_drive_path, substituted_path);
  EXPECT_FALSE(download_handler_->IsDriveDownload(&download_item_));
}

TEST_F(DownloadHandlerTest, SubstituteDriveDownloadPath) {
  const base::FilePath drive_path =
      util::GetDriveMountPointPath().AppendASCII("test.dat");

  // Test the case that the download target directory already exists.
  test_file_system_.set_download_path_state(PATH_EXISTS);

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      drive_path,
      &download_item_,
      google_apis::test_util::CreateCopyResultCallback(&substituted_path));
  test_util::RunBlockingPoolTask();

  // Check the result.
  EXPECT_TRUE(temp_dir_.path().IsParent(substituted_path));
  ASSERT_TRUE(download_handler_->IsDriveDownload(&download_item_));
  EXPECT_EQ(drive_path, download_handler_->GetTargetPath(&download_item_));
}

TEST_F(DownloadHandlerTest, SubstituteDriveDownloadPathGetEntryFailure) {
  const base::FilePath drive_path =
      util::GetDriveMountPointPath().AppendASCII("test.dat");

  // Test the case that access to the download target directory failed for some
  // reason.
  test_file_system_.set_download_path_state(PATH_INVALID);

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      drive_path,
      &download_item_,
      google_apis::test_util::CreateCopyResultCallback(&substituted_path));
  test_util::RunBlockingPoolTask();

  // Check the result.
  EXPECT_TRUE(substituted_path.empty());
}

TEST_F(DownloadHandlerTest, SubstituteDriveDownloadPathCreateDirectory) {
  const base::FilePath drive_path =
      util::GetDriveMountPointPath().AppendASCII("test.dat");

  // Test the case that access to the download target directory does not exist,
  // and thus will be created in DownloadHandler.
  test_file_system_.set_download_path_state(PATH_NOT_EXIST);

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      drive_path,
      &download_item_,
      google_apis::test_util::CreateCopyResultCallback(&substituted_path));
  test_util::RunBlockingPoolTask();

  // Check the result.
  EXPECT_TRUE(temp_dir_.path().IsParent(substituted_path));
  ASSERT_TRUE(download_handler_->IsDriveDownload(&download_item_));
  EXPECT_EQ(drive_path, download_handler_->GetTargetPath(&download_item_));
}

TEST_F(DownloadHandlerTest,
       SubstituteDriveDownloadPathCreateDirectoryFailure) {
  const base::FilePath drive_path =
      util::GetDriveMountPointPath().AppendASCII("test.dat");

  // Test the case that access to the download target directory does not exist,
  // and creation fails for some reason.
  test_file_system_.set_download_path_state(PATH_NOT_EXIST_AND_CREATE_FAIL);

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      drive_path,
      &download_item_,
      google_apis::test_util::CreateCopyResultCallback(&substituted_path));
  test_util::RunBlockingPoolTask();

  // Check the result.
  EXPECT_TRUE(substituted_path.empty());
}

// content::SavePackage calls SubstituteDriveDownloadPath before creating
// DownloadItem.
TEST_F(DownloadHandlerTest, SubstituteDriveDownloadPathForSavePackage) {
  const base::FilePath drive_path =
      util::GetDriveMountPointPath().AppendASCII("test.dat");
  test_file_system_.set_download_path_state(PATH_EXISTS);

  // Call SubstituteDriveDownloadPath()
  base::FilePath substituted_path;
  download_handler_->SubstituteDriveDownloadPath(
      drive_path,
      NULL,  // DownloadItem is not available at this moment.
      google_apis::test_util::CreateCopyResultCallback(&substituted_path));
  test_util::RunBlockingPoolTask();

  // Check the result of SubstituteDriveDownloadPath().
  EXPECT_TRUE(temp_dir_.path().IsParent(substituted_path));

  // |download_item_| is not a drive download yet.
  EXPECT_FALSE(download_handler_->IsDriveDownload(&download_item_));

  // Call SetDownloadParams().
  download_handler_->SetDownloadParams(drive_path, &download_item_);

  // |download_item_| is a drive download now.
  ASSERT_TRUE(download_handler_->IsDriveDownload(&download_item_));
  EXPECT_EQ(drive_path, download_handler_->GetTargetPath(&download_item_));
}

TEST_F(DownloadHandlerTest, CheckForFileExistence) {
  const base::FilePath drive_path =
      util::GetDriveMountPointPath().AppendASCII("test.dat");

  // Make |download_item_| a drive download.
  download_handler_->SetDownloadParams(drive_path, &download_item_);
  ASSERT_TRUE(download_handler_->IsDriveDownload(&download_item_));
  EXPECT_EQ(drive_path, download_handler_->GetTargetPath(&download_item_));

  // Test for the case when the path exists.
  test_file_system_.set_download_path_state(PATH_EXISTS);

  // Call CheckForFileExistence.
  bool file_exists = false;
  download_handler_->CheckForFileExistence(
      &download_item_,
      google_apis::test_util::CreateCopyResultCallback(&file_exists));
  test_util::RunBlockingPoolTask();

  // Check the result.
  EXPECT_TRUE(file_exists);

  // Test for the case when the path does not exist.
  test_file_system_.set_download_path_state(PATH_NOT_EXIST);

  // Call CheckForFileExistence again.
  download_handler_->CheckForFileExistence(
      &download_item_,
      google_apis::test_util::CreateCopyResultCallback(&file_exists));
  test_util::RunBlockingPoolTask();

  // Check the result.
  EXPECT_FALSE(file_exists);
}

}  // namespace drive
