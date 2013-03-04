// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"

using ::testing::_;
using ::testing::ReturnRef;

using chromeos::disks::DiskMountManager;

namespace {

struct TestDiskInfo {
  const char* system_path;
  const char* file_path;
  const char* device_label;
  const char* drive_label;
  const char* vendor_id;
  const char* vendor_name;
  const char* product_id;
  const char* product_name;
  const char* fs_uuid;
  const char* system_path_prefix;
  chromeos::DeviceType device_type;
  uint64 size_in_bytes;
  bool is_parent;
  bool is_read_only;
  bool has_media;
  bool on_boot_device;
  bool is_hidden;
};

struct TestMountPoint {
  std::string source_path;
  std::string mount_path;
  chromeos::MountType mount_type;
  chromeos::disks::MountCondition mount_condition;

  // -1 if there is no disk info.
  int disk_info_index;
};

TestDiskInfo kTestDisks[] = {
  {
    "system_path1",
    "file_path1",
    "device_label1",
    "drive_label1",
    "0123",
    "vendor1",
    "abcd",
    "product1",
    "FFFF-FFFF",
    "system_path_prefix1",
    chromeos::DEVICE_TYPE_USB,
    1073741824,
    false,
    false,
    false,
    false,
    false
  },
  {
    "system_path2",
    "file_path2",
    "device_label2",
    "drive_label2",
    "4567",
    "vendor2",
    "cdef",
    "product2",
    "0FFF-FFFF",
    "system_path_prefix2",
    chromeos::DEVICE_TYPE_MOBILE,
    47723,
    true,
    true,
    true,
    true,
    false
  },
  {
    "system_path3",
    "file_path3",
    "device_label3",
    "drive_label3",
    "89ab",
    "vendor3",
    "ef01",
    "product3",
    "00FF-FFFF",
    "system_path_prefix3",
    chromeos::DEVICE_TYPE_OPTICAL_DISC,
    0,
    true,
    false,
    false,
    true,
    false
  }
};

}  // namespace

class ExtensionFileBrowserPrivateApiTest : public ExtensionApiTest {
 public:
  ExtensionFileBrowserPrivateApiTest()
      : disk_mount_manager_mock_(NULL) {
    InitMountPoints();
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

    // OVERRIDE FindDiskBySourcePath mock function.
    ON_CALL(*disk_mount_manager_mock_, FindDiskBySourcePath(_)).
        WillByDefault(Invoke(
            this, &ExtensionFileBrowserPrivateApiTest::FindVolumeBySourcePath));
  }

  // ExtensionApiTest override
  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    chromeos::disks::DiskMountManager::Shutdown();
    disk_mount_manager_mock_ = NULL;

    ExtensionApiTest::TearDownInProcessBrowserTestFixture();
  }

 private:
  void InitMountPoints() {
    const TestMountPoint kTestMountPoints[] = {
      {
        "device_path1",
        chromeos::CrosDisksClient::GetRemovableDiskMountPoint().AppendASCII(
            "mount_path1").AsUTF8Unsafe(),
        chromeos::MOUNT_TYPE_DEVICE,
        chromeos::disks::MOUNT_CONDITION_NONE,
        0
      },
      {
        "device_path2",
        chromeos::CrosDisksClient::GetRemovableDiskMountPoint().AppendASCII(
            "mount_path2").AsUTF8Unsafe(),
        chromeos::MOUNT_TYPE_DEVICE,
        chromeos::disks::MOUNT_CONDITION_NONE,
        1
      },
      {
        "device_path3",
        chromeos::CrosDisksClient::GetRemovableDiskMountPoint().AppendASCII(
            "mount_path3").AsUTF8Unsafe(),
        chromeos::MOUNT_TYPE_DEVICE,
        chromeos::disks::MOUNT_CONDITION_NONE,
        2
      },
      {
        "archive_path",
        chromeos::CrosDisksClient::GetArchiveMountPoint().AppendASCII(
            "archive_mount_path").AsUTF8Unsafe(),
        chromeos::MOUNT_TYPE_ARCHIVE,
        chromeos::disks::MOUNT_CONDITION_NONE,
        -1
      }
    };

    for (size_t i = 0; i < arraysize(kTestMountPoints); i++) {
      mount_points_.insert(DiskMountManager::MountPointMap::value_type(
          kTestMountPoints[i].mount_path,
          DiskMountManager::MountPointInfo(kTestMountPoints[i].source_path,
                                           kTestMountPoints[i].mount_path,
                                           kTestMountPoints[i].mount_type,
                                           kTestMountPoints[i].mount_condition)
      ));
      int disk_info_index = kTestMountPoints[i].disk_info_index;
      if (kTestMountPoints[i].disk_info_index >= 0) {
        EXPECT_GT(arraysize(kTestDisks), static_cast<size_t>(disk_info_index));
        if (static_cast<size_t>(disk_info_index) >= arraysize(kTestDisks))
          return;

        volumes_.insert(DiskMountManager::DiskMap::value_type(
            kTestMountPoints[i].source_path,
            new DiskMountManager::Disk(
                kTestMountPoints[i].source_path,
                kTestMountPoints[i].mount_path,
                kTestDisks[disk_info_index].system_path,
                kTestDisks[disk_info_index].file_path,
                kTestDisks[disk_info_index].device_label,
                kTestDisks[disk_info_index].drive_label,
                kTestDisks[disk_info_index].vendor_id,
                kTestDisks[disk_info_index].vendor_name,
                kTestDisks[disk_info_index].product_id,
                kTestDisks[disk_info_index].product_name,
                kTestDisks[disk_info_index].fs_uuid,
                kTestDisks[disk_info_index].system_path_prefix,
                kTestDisks[disk_info_index].device_type,
                kTestDisks[disk_info_index].size_in_bytes,
                kTestDisks[disk_info_index].is_parent,
                kTestDisks[disk_info_index].is_read_only,
                kTestDisks[disk_info_index].has_media,
                kTestDisks[disk_info_index].on_boot_device,
                kTestDisks[disk_info_index].is_hidden
            )
        ));
      }
    }
  }

  const DiskMountManager::Disk* FindVolumeBySourcePath(
      const std::string& source_path) {
    DiskMountManager::DiskMap::const_iterator volume_it =
        volumes_.find(source_path);
    return (volume_it == volumes_.end()) ? NULL : volume_it->second;
  }

 protected:
  chromeos::disks::MockDiskMountManager* disk_mount_manager_mock_;
  DiskMountManager::DiskMap volumes_;
  DiskMountManager::MountPointMap mount_points_;
};

IN_PROC_BROWSER_TEST_F(ExtensionFileBrowserPrivateApiTest, FileBrowserMount) {
  // We will call fileBrowserPrivate.unmountVolume once. To test that method, we
  // check that UnmountPath is really called with the same value.
  EXPECT_CALL(*disk_mount_manager_mock_, UnmountPath(_, _))
      .Times(0);
  EXPECT_CALL(*disk_mount_manager_mock_,
              UnmountPath(
                  chromeos::CrosDisksClient::GetArchiveMountPoint().AppendASCII(
                      "archive_mount_path").AsUTF8Unsafe(),
                  chromeos::UNMOUNT_OPTIONS_NONE)).Times(1);

  EXPECT_CALL(*disk_mount_manager_mock_, disks())
      .WillRepeatedly(ReturnRef(volumes_));

  EXPECT_CALL(*disk_mount_manager_mock_, mount_points())
      .WillRepeatedly(ReturnRef(mount_points_));

  ASSERT_TRUE(RunComponentExtensionTest("filebrowser_mount"))  << message_;
}
