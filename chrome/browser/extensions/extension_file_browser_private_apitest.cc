// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mock_mount_library.h"
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
  ExtensionFileBrowserPrivateApiTest() : test_mount_point_("/tmp") {
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
        std::pair<std::string, chromeos::MountLibrary::Disk*>(
            "device_path1",
            new chromeos::MountLibrary::Disk("device_path1",
                                             "/media/removable/mount_path1",
                                             "system_path1",
                                             "file_path1",
                                             "device_label1",
                                             "drive_label1",
                                             "parent_path1",
                                             "system_path_prefix1",
                                             chromeos::FLASH,
                                             1073741824,
                                             false,
                                             false,
                                             false,
                                             false,
                                             false)));
    volumes_.insert(
        std::pair<std::string, chromeos::MountLibrary::Disk*>(
            "device_path2",
            new chromeos::MountLibrary::Disk("device_path2",
                                             "/media/removable/mount_path2",
                                             "system_path2",
                                             "file_path2",
                                             "device_label2",
                                             "drive_label2",
                                             "parent_path2",
                                             "system_path_prefix2",
                                             chromeos::HDD,
                                             47723,
                                             true,
                                             true,
                                             true,
                                             true,
                                             false)));
    volumes_.insert(
        std::pair<std::string, chromeos::MountLibrary::Disk*>(
            "device_path3",
            new chromeos::MountLibrary::Disk("device_path3",
                                             "/media/removable/mount_path3",
                                             "system_path3",
                                             "file_path3",
                                             "device_label3",
                                             "drive_label3",
                                             "parent_path3",
                                             "system_path_prefix3",
                                             chromeos::OPTICAL,
                                             0,
                                             true,
                                             false,
                                             false,
                                             true,
                                             false)));
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

 private:
  FilePath test_mount_point_;
};

IN_PROC_BROWSER_TEST_F(ExtensionFileBrowserPrivateApiTest, FileBrowserMount) {
  // We will call fileBrowserPrivate.unmountVolume once. To test that method, we
  // check that UnmountPath is really called with the same value.
  AddTmpMountPoint();
  EXPECT_CALL(mount_library_mock_, UnmountPath(_))
      .Times(0);
  EXPECT_CALL(mount_library_mock_, UnmountPath(StrEq("/tmp/test_file.zip")))
      .Times(1);

  EXPECT_CALL(mount_library_mock_, disks())
      .WillRepeatedly(ReturnRef(volumes_));

  ASSERT_TRUE(RunComponentExtensionTest("filebrowser_mount"))  << message_;
}

