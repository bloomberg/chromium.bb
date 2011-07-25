// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mock_mount_library.h"
#include "chrome/browser/extensions/extension_apitest.h"

using ::testing::_;
using ::testing::ReturnRef;
using ::testing::StrEq;

class ExtensionFileBrowserPrivateApiTest : public ExtensionApiTest {
 public:
  ExtensionFileBrowserPrivateApiTest() {
    mount_library_mock_.SetupDefaultReplies();

    chromeos::CrosLibrary::Get()->GetTestApi()->SetMountLibrary(
        &mount_library_mock_,
        false);  // We own the mock library object.

    CreateVolumeMap();
  }

  virtual ~ExtensionFileBrowserPrivateApiTest() {
    DeleteVolumeMap();
    chromeos::CrosLibrary::Get()->GetTestApi()->SetMountLibrary(NULL, true);
  }

 private:
  void CreateVolumeMap() {
    // These have to be sync'd with values in filebrowser_mount extension.
    volumes_.insert(
        std::pair<std::string, chromeos::MountLibrary::Disk*>(
            "device_path1",
            new chromeos::MountLibrary::Disk("device_path1",
                                             "mount_path1",
                                             "system_path1",
                                             "file_path1",
                                             "device_label1",
                                             "drive_label1",
                                             "parent_path1",
                                             chromeos::FLASH,
                                             1073741824,
                                             false,
                                             false,
                                             false,
                                             false)));
    volumes_.insert(
        std::pair<std::string, chromeos::MountLibrary::Disk*>(
            "device_path2",
            new chromeos::MountLibrary::Disk("device_path2",
                                             "mount_path2",
                                             "system_path2",
                                             "file_path2",
                                             "device_label2",
                                             "drive_label2",
                                             "parent_path2",
                                             chromeos::HDD,
                                             47723,
                                             true,
                                             true,
                                             true,
                                             true)));
    volumes_.insert(
        std::pair<std::string, chromeos::MountLibrary::Disk*>(
            "device_path3",
            new chromeos::MountLibrary::Disk("device_path3",
                                             "mount_path3",
                                             "system_path3",
                                             "file_path3",
                                             "device_label3",
                                             "drive_label3",
                                             "parent_path3",
                                             chromeos::OPTICAL,
                                             0,
                                             true,
                                             false,
                                             false,
                                             true)));
  }

  void DeleteVolumeMap() {
    for (chromeos::MountLibrary::DiskMap::iterator it = volumes_.begin();
         it != volumes_.end();
         ++it) {
      delete it->second;
    }
    volumes_.clear();
  }

 protected:
  chromeos::MockMountLibrary mount_library_mock_;
  chromeos::MountLibrary::DiskMap volumes_;
};

IN_PROC_BROWSER_TEST_F(ExtensionFileBrowserPrivateApiTest, FileBrowserMount) {
  // We will call fileBrowserPrivate.unmountVolume once. To test that method, we
  // check that UnmountPath is really called with the same value.
  EXPECT_CALL(mount_library_mock_, UnmountPath(StrEq("devicePath1")))
      .Times(1);

  EXPECT_CALL(mount_library_mock_, disks())
      .WillRepeatedly(ReturnRef(volumes_));

  ASSERT_TRUE(RunComponentExtensionTest("filebrowser_mount"))  << message_;
}

