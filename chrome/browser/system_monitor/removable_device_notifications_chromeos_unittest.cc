// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chromeos::RemovableDeviceNotificationsCros unit tests.

#include "chrome/browser/system_monitor/removable_device_notifications_chromeos.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/system_monitor/system_monitor.h"
#include "base/test/mock_devices_changed_observer.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

using content::BrowserThread;
using disks::DiskMountManager;
using testing::_;

const char kDevice1[] = "/dev/d1";
const char kDevice2[] = "/dev/disk/d2";
const char kDevice1Name[] = "d1";
const char kDevice2Name[] = "d2";
const char kMountPointA[] = "mnt_a";
const char kMountPointB[] = "mnt_b";

std::string GetDCIMDeviceId(const std::string& unique_id) {
  return chrome::MediaStorageUtil::MakeDeviceId(
      chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM, unique_id);
}

class RemovableDeviceNotificationsCrosTest : public testing::Test {
 public:
  RemovableDeviceNotificationsCrosTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        file_thread_(BrowserThread::FILE) {
  }
  virtual ~RemovableDeviceNotificationsCrosTest() {}

 protected:
  virtual void SetUp() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    file_thread_.Start();

    mock_devices_changed_observer_.reset(new base::MockDevicesChangedObserver);
    system_monitor_.AddDevicesChangedObserver(
        mock_devices_changed_observer_.get());

    disk_mount_manager_mock_ = new disks::MockDiskMountManager();
    DiskMountManager::InitializeForTesting(disk_mount_manager_mock_);
    disk_mount_manager_mock_->SetupDefaultReplies();

    // Initialize the test subject.
    notifications_ = new RemovableDeviceNotificationsCros();
  }

  virtual void TearDown() {
    notifications_ = NULL;
    disk_mount_manager_mock_ = NULL;
    DiskMountManager::Shutdown();
    system_monitor_.RemoveDevicesChangedObserver(
        mock_devices_changed_observer_.get());
    WaitForFileThread();
  }

  base::MockDevicesChangedObserver& observer() {
    return *mock_devices_changed_observer_;
  }

  void MountDevice(MountError error_code,
                   const DiskMountManager::MountPointInfo& mount_info,
                   const std::string& unique_id) {
    if (error_code == MOUNT_ERROR_NONE) {
      disk_mount_manager_mock_->CreateDiskEntryForMountDevice(
        mount_info, unique_id);
    }
    notifications_->MountCompleted(disks::DiskMountManager::MOUNTING,
                                   error_code,
                                   mount_info);
    WaitForFileThread();
  }

  void UnmountDevice(MountError error_code,
                     const DiskMountManager::MountPointInfo& mount_info) {
    notifications_->MountCompleted(disks::DiskMountManager::UNMOUNTING,
                                   error_code,
                                   mount_info);
    if (error_code == MOUNT_ERROR_NONE) {
      disk_mount_manager_mock_->RemoveDiskEntryForMountDevice(
          mount_info);
    }
    WaitForFileThread();
  }

  // Create a directory named |dir| relative to the test directory.
  // Set |with_dcim_dir| to true if the created directory will have a "DCIM"
  // subdirectory.
  // Returns the full path to the created directory on success, or an empty
  // path on failure.
  FilePath CreateMountPoint(const std::string& dir, bool with_dcim_dir) {
    FilePath return_path(scoped_temp_dir_.path());
    return_path = return_path.AppendASCII(dir);
    FilePath path(return_path);
    if (with_dcim_dir)
      path = path.AppendASCII("DCIM");
    if (!file_util::CreateDirectory(path))
      return FilePath();
    return return_path;
  }

  static void PostQuitToUIThread() {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            MessageLoop::QuitClosure());
  }

  static void WaitForFileThread() {
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&PostQuitToUIThread));
    MessageLoop::current()->Run();
  }

 private:
  // The message loops and threads to run tests on.
  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  // Temporary directory for created test data.
  ScopedTempDir scoped_temp_dir_;

  // Objects that talks with RemovableDeviceNotificationsCros.
  base::SystemMonitor system_monitor_;
  scoped_ptr<base::MockDevicesChangedObserver> mock_devices_changed_observer_;
  // Owned by DiskMountManager.
  disks::MockDiskMountManager* disk_mount_manager_mock_;

  scoped_refptr<RemovableDeviceNotificationsCros> notifications_;

  DISALLOW_COPY_AND_ASSIGN(RemovableDeviceNotificationsCrosTest);
};

// Simple test case where we attach and detach a media device.
TEST_F(RemovableDeviceNotificationsCrosTest, BasicAttachDetach) {
  testing::Sequence mock_sequence;
  FilePath mount_path1 = CreateMountPoint(kMountPointA, true);
  ASSERT_FALSE(mount_path1.empty());
  DiskMountManager::MountPointInfo mount_info(kDevice1,
                                              mount_path1.value(),
                                              MOUNT_TYPE_DEVICE,
                                              disks::MOUNT_CONDITION_NONE);
  const std::string kUniqueId0 = "FFFF-FFFF";
  EXPECT_CALL(observer(),
              OnRemovableStorageAttached(GetDCIMDeviceId(kUniqueId0),
                                         ASCIIToUTF16(kDevice1Name),
                                         mount_path1.value()))
      .InSequence(mock_sequence);
  MountDevice(MOUNT_ERROR_NONE, mount_info, kUniqueId0);

  EXPECT_CALL(observer(),
              OnRemovableStorageDetached(GetDCIMDeviceId(kUniqueId0)))
      .InSequence(mock_sequence);
  UnmountDevice(MOUNT_ERROR_NONE, mount_info);

  FilePath mount_path2 = CreateMountPoint(kMountPointB, true);
  ASSERT_FALSE(mount_path2.empty());
  DiskMountManager::MountPointInfo mount_info2(kDevice2,
                                               mount_path2.value(),
                                               MOUNT_TYPE_DEVICE,
                                               disks::MOUNT_CONDITION_NONE);
  const std::string kUniqueId1 = "FFF0-FFF0";

  EXPECT_CALL(observer(),
              OnRemovableStorageAttached(GetDCIMDeviceId(kUniqueId1),
                                         ASCIIToUTF16(kDevice2Name),
                                         mount_path2.value()))
      .InSequence(mock_sequence);
  MountDevice(MOUNT_ERROR_NONE, mount_info2, kUniqueId1);

  EXPECT_CALL(observer(),
              OnRemovableStorageDetached(GetDCIMDeviceId(kUniqueId1)))
      .InSequence(mock_sequence);
  UnmountDevice(MOUNT_ERROR_NONE, mount_info2);
}

// Removable mass storage devices with no dcim folder are also recognized.
TEST_F(RemovableDeviceNotificationsCrosTest, NoDCIM) {
  testing::Sequence mock_sequence;
  FilePath mount_path = CreateMountPoint(kMountPointA, false);
  const std::string kUniqueId = "FFFF-FFFF";
  ASSERT_FALSE(mount_path.empty());
  DiskMountManager::MountPointInfo mount_info(kDevice1,
                                              mount_path.value(),
                                              MOUNT_TYPE_DEVICE,
                                              disks::MOUNT_CONDITION_NONE);
  const std::string device_id = chrome::MediaStorageUtil::MakeDeviceId(
      chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM, kUniqueId);
  EXPECT_CALL(observer(),
              OnRemovableStorageAttached(device_id, ASCIIToUTF16(kDevice1Name),
                                         mount_path.value())).Times(1);
  MountDevice(MOUNT_ERROR_NONE, mount_info, kUniqueId);
}

// Non device mounts and mount errors are ignored.
TEST_F(RemovableDeviceNotificationsCrosTest, Ignore) {
  testing::Sequence mock_sequence;
  FilePath mount_path = CreateMountPoint(kMountPointA, true);
  const std::string kUniqueId = "FFFF-FFFF";
  ASSERT_FALSE(mount_path.empty());

  // Mount error.
  DiskMountManager::MountPointInfo mount_info(kDevice1,
                                              mount_path.value(),
                                              MOUNT_TYPE_DEVICE,
                                              disks::MOUNT_CONDITION_NONE);
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(0);
  MountDevice(MOUNT_ERROR_UNKNOWN, mount_info, kUniqueId);

  // Not a device
  mount_info.mount_type = MOUNT_TYPE_ARCHIVE;
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(0);
  MountDevice(MOUNT_ERROR_NONE, mount_info, kUniqueId);

  // Unsupported file system.
  mount_info.mount_type = MOUNT_TYPE_DEVICE;
  mount_info.mount_condition = disks::MOUNT_CONDITION_UNSUPPORTED_FILESYSTEM;
  EXPECT_CALL(observer(), OnRemovableStorageAttached(_, _, _)).Times(0);
  MountDevice(MOUNT_ERROR_NONE, mount_info, kUniqueId);
}

}  // namespace

}  // namespace chrome
