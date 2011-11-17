// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/stl_util.h"
#include "chrome/browser/chromeos/disks/mock_disk_mount_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_path_manager.h"

using ::testing::_;
using ::testing::ReturnRef;
using ::testing::StrEq;

class ExtensionFileBrowserPrivateApiTest : public ExtensionApiTest {
 public:
  ExtensionFileBrowserPrivateApiTest()
      : disk_mount_manager_mock_(NULL),
        test_mount_point_("/tmp") {
    CreateVolumeMap();
  }

  virtual ~ExtensionFileBrowserPrivateApiTest() {
    DCHECK(!disk_mount_manager_mock_);
    STLDeleteValues(&volumes_);
  }

  // ExtensionApiTest override
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    disk_mount_manager_mock_ = new chromeos::disks::MockDiskMountManager;
    chromeos::disks::DiskMountManager::InitializeForTesting(
        disk_mount_manager_mock_);
    disk_mount_manager_mock_->SetupDefaultReplies();
  }

  // ExtensionApiTest override
  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    chromeos::disks::DiskMountManager::Shutdown();
    disk_mount_manager_mock_ = NULL;

    ExtensionApiTest::TearDownInProcessBrowserTestFixture();
  }

  void AddTmpMountPoint() {
    fileapi::FileSystemPathManager* path_manager =
        browser()->profile()->GetFileSystemContext()->path_manager();
    fileapi::ExternalFileSystemMountPointProvider* provider =
        path_manager->external_provider();
    provider->AddMountPoint(test_mount_point_);
  }

 private:
  void CreateVolumeMap() {
    // These have to be sync'd with values in filebrowser_mount extension.
    volumes_.insert(
        std::pair<std::string, chromeos::disks::DiskMountManager::Disk*>(
            "device_path1",
            new chromeos::disks::DiskMountManager::Disk(
                "device_path1",
                "/media/removable/mount_path1",
                "system_path1",
                "file_path1",
                "device_label1",
                "drive_label1",
                "system_path_prefix1",
                chromeos::FLASH,
                1073741824,
                false,
                false,
                false,
                false,
                false)));
    volumes_.insert(
        std::pair<std::string, chromeos::disks::DiskMountManager::Disk*>(
            "device_path2",
            new chromeos::disks::DiskMountManager::Disk(
                "device_path2",
                "/media/removable/mount_path2",
                "system_path2",
                "file_path2",
                "device_label2",
                "drive_label2",
                "system_path_prefix2",
                chromeos::HDD,
                47723,
                true,
                true,
                true,
                true,
                false)));
    volumes_.insert(
        std::pair<std::string, chromeos::disks::DiskMountManager::Disk*>(
            "device_path3",
            new chromeos::disks::DiskMountManager::Disk(
                "device_path3",
                "/media/removable/mount_path3",
                "system_path3",
                "file_path3",
                "device_label3",
                "drive_label3",
                "system_path_prefix3",
                chromeos::OPTICAL,
                0,
                true,
                false,
                false,
                true,
                false)));
  }

 protected:
  chromeos::disks::MockDiskMountManager* disk_mount_manager_mock_;
  chromeos::disks::DiskMountManager::DiskMap volumes_;

 private:
  FilePath test_mount_point_;
};

IN_PROC_BROWSER_TEST_F(ExtensionFileBrowserPrivateApiTest, FileBrowserMount) {
  // We will call fileBrowserPrivate.unmountVolume once. To test that method, we
  // check that UnmountPath is really called with the same value.
  AddTmpMountPoint();
  EXPECT_CALL(*disk_mount_manager_mock_, UnmountPath(_))
      .Times(0);
  EXPECT_CALL(*disk_mount_manager_mock_,
              UnmountPath(StrEq("/tmp/test_file.zip"))).Times(1);

  EXPECT_CALL(*disk_mount_manager_mock_, disks())
      .WillRepeatedly(ReturnRef(volumes_));

  ASSERT_TRUE(RunComponentExtensionTest("filebrowser_mount"))  << message_;
}
