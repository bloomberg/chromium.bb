// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/mounted_disk_monitor.h"

#include "base/basictypes.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/file_manager/fake_disk_mount_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace {

// Creates a fake disk with |device_path| and |fs_uuid|.
scoped_ptr<chromeos::disks::DiskMountManager::Disk> CreateDisk(
    const std::string& device_path,
    const std::string& fs_uuid) {
  return make_scoped_ptr(
      new chromeos::disks::DiskMountManager::Disk(
          device_path, "", "", "", "", "", "", "", "", "", fs_uuid, "",
          chromeos::DEVICE_TYPE_USB, 0, false, false, false, false, false));
}

}  // namespace

class MountedDiskMonitorTest : public testing::Test {
 protected:
  MountedDiskMonitorTest() {
  }

  virtual ~MountedDiskMonitorTest() {
  }

  virtual void SetUp() OVERRIDE {
    power_manager_client_.reset(new chromeos::FakePowerManagerClient);
    disk_mount_manager_.reset(new FakeDiskMountManager);
    mounted_disk_monitor_.reset(new MountedDiskMonitor(
        power_manager_client_.get(),
        disk_mount_manager_.get()));
    mounted_disk_monitor_->set_resuming_time_span_for_testing(
        base::TimeDelta::FromSeconds(0));
  }

  base::MessageLoop message_loop_;
  scoped_ptr<chromeos::FakePowerManagerClient> power_manager_client_;
  scoped_ptr<FakeDiskMountManager> disk_mount_manager_;
  scoped_ptr<MountedDiskMonitor> mounted_disk_monitor_;
};

// Makes sure that just mounting and unmounting repeatedly doesn't affect to
// "remounting" state.
TEST_F(MountedDiskMonitorTest, WithoutSuspend) {
  scoped_ptr<chromeos::disks::DiskMountManager::Disk> disk(
      CreateDisk("removable_device1", "uuid1"));

  chromeos::disks::DiskMountManager::Disk* disk_ptr = disk.get();

  const chromeos::disks::DiskMountManager::MountPointInfo kMountPoint(
      "removable_device1", "/tmp/removable_device1",
      chromeos::MOUNT_TYPE_DEVICE, chromeos::disks::MOUNT_CONDITION_NONE);

  ASSERT_TRUE(disk_mount_manager_->AddDiskForTest(disk.release()));

  // First, the disk is not remounting.
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk_ptr));

  // Simple mounting and unmounting doesn't affect remounting state.
  mounted_disk_monitor_->OnMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      kMountPoint);
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk_ptr));

  mounted_disk_monitor_->OnMountEvent(
      chromeos::disks::DiskMountManager::UNMOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      kMountPoint);
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk_ptr));

  // Mounting again also should not affect remounting state.
  mounted_disk_monitor_->OnMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      kMountPoint);
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk_ptr));
}

// Makes sure that the unmounting after system resuming triggers the
// "remounting" state, then after some period, the state is reset.
TEST_F(MountedDiskMonitorTest, SuspendAndResume) {
  scoped_ptr<chromeos::disks::DiskMountManager::Disk> disk1(
      CreateDisk("removable_device1", "uuid1"));
  scoped_ptr<chromeos::disks::DiskMountManager::Disk> disk2(
      CreateDisk("removable_device2", "uuid2"));

  chromeos::disks::DiskMountManager::Disk* disk1_ptr = disk1.get();
  chromeos::disks::DiskMountManager::Disk* disk2_ptr = disk2.get();

  const chromeos::disks::DiskMountManager::MountPointInfo kMountPoint1(
      "removable_device1", "/tmp/removable_device1",
      chromeos::MOUNT_TYPE_DEVICE, chromeos::disks::MOUNT_CONDITION_NONE);
  const chromeos::disks::DiskMountManager::MountPointInfo kMountPoint2(
      "removable_device2", "/tmp/removable_device2",
      chromeos::MOUNT_TYPE_DEVICE, chromeos::disks::MOUNT_CONDITION_NONE);

  ASSERT_TRUE(disk_mount_manager_->AddDiskForTest(disk1.release()));
  ASSERT_TRUE(disk_mount_manager_->AddDiskForTest(disk2.release()));

  // Mount |disk1|.
  mounted_disk_monitor_->OnMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      kMountPoint1);
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk1_ptr));

  // Pseudo system suspend and resume.
  mounted_disk_monitor_->SuspendImminent();
  mounted_disk_monitor_->SystemResumed(base::TimeDelta::FromSeconds(0));

  // On system resume, we expect unmount and then mount immediately.
  // During the phase, we expect the disk is remounting.
  mounted_disk_monitor_->OnMountEvent(
      chromeos::disks::DiskMountManager::UNMOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      kMountPoint1);
  EXPECT_TRUE(mounted_disk_monitor_->DiskIsRemounting(*disk1_ptr));

  mounted_disk_monitor_->OnMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      kMountPoint1);
  EXPECT_TRUE(mounted_disk_monitor_->DiskIsRemounting(*disk1_ptr));

  // New disk should not be "remounting."
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk2_ptr));
  mounted_disk_monitor_->OnMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      kMountPoint2);
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk2_ptr));

  // After certain period, remounting state should be cleared.
  base::RunLoop().RunUntilIdle();  // Emulate time passage.
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk1_ptr));
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk2_ptr));
}

}  // namespace file_manager
