// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/fake_drive_file_system.h"

#include "base/message_loop.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace test_util {

class FakeDriveFileSystemTest : public ::testing::Test {
 protected:
  FakeDriveFileSystemTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    // Initialize FakeDriveService.
    fake_drive_service_.reset(new google_apis::FakeDriveService);
    fake_drive_service_->LoadResourceListForWapi(
        "chromeos/gdata/root_feed.json");
    fake_drive_service_->LoadAccountMetadataForWapi(
        "chromeos/gdata/account_metadata.json");
    fake_drive_service_->LoadAppListForDriveApi("chromeos/drive/applist.json");

    // Create a testee instance.
    fake_drive_file_system_.reset(
        new FakeDriveFileSystem(fake_drive_service_.get()));
  }

  MessageLoopForUI message_loop_;
  // The order of the test threads is important, do not change the order.
  // See also content/browser/browser_thread_impl.cc.
  content::TestBrowserThread ui_thread_;

  scoped_ptr<google_apis::FakeDriveService> fake_drive_service_;
  scoped_ptr<FakeDriveFileSystem> fake_drive_file_system_;
};

TEST_F(FakeDriveFileSystemTest, GetEntryInfoByResourceId) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<DriveEntryProto> entry;
  base::FilePath file_path;

  fake_drive_file_system_->GetEntryInfoByResourceId(
      "folder:sub_dir_folder_resource_id",
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &entry));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(
      util::GetDriveMyDriveRootPath().AppendASCII(
          "Directory 1/Sub Directory Folder"),
      file_path);
  EXPECT_TRUE(entry);  // Just make sure something is returned.
}

}  // namespace test_util
}  // namespace drive
