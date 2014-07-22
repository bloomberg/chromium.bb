// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/mounted_disk_monitor.h"

#include "base/basictypes.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
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
          chromeos::DEVICE_TYPE_USB, 0, false, false, false, false, false,
          false));
}

}  // namespace

class MountedDiskMonitorTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    power_manager_client_.reset(new chromeos::FakePowerManagerClient);
    mounted_disk_monitor_.reset(new MountedDiskMonitor(
        power_manager_client_.get()));
    mounted_disk_monitor_->set_resuming_time_span_for_testing(
        base::TimeDelta::FromSeconds(0));
  }

  base::MessageLoop message_loop_;
  scoped_ptr<chromeos::FakePowerManagerClient> power_manager_client_;
  scoped_ptr<MountedDiskMonitor> mounted_disk_monitor_;
};

// Makes sure that just mounting and unmounting repeatedly doesn't affect to
// "remounting" state.
TEST_F(MountedDiskMonitorTest, WithoutSuspend) {
  scoped_ptr<chromeos::disks::DiskMountManager::Disk> disk(
      CreateDisk("removable_device1", "uuid1"));

  const chromeos::disks::DiskMountManager::MountPointInfo kMountPoint(
      "removable_device1", "/tmp/removable_device1",
      chromeos::MOUNT_TYPE_DEVICE, chromeos::disks::MOUNT_CONDITION_NONE);

  // First, the disk is not remounting.
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk));

  // Simple mounting and unmounting doesn't affect remounting state.
  mounted_disk_monitor_->OnMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      kMountPoint,
      disk.get());
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk));

  mounted_disk_monitor_->OnMountEvent(
      chromeos::disks::DiskMountManager::UNMOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      kMountPoint,
      disk.get());
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk));

  // Mounting again also should not affect remounting state.
  mounted_disk_monitor_->OnMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      kMountPoint,
      disk.get());
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk));
}

// Makes sure that the unmounting after system resuming triggers the
// "remounting" state, then after some period, the state is reset.
TEST_F(MountedDiskMonitorTest, SuspendAndResume) {
  scoped_ptr<chromeos::disks::DiskMountManager::Disk> disk1(
      CreateDisk("removable_device1", "uuid1"));
  scoped_ptr<chromeos::disks::DiskMountManager::Disk> disk2(
      CreateDisk("removable_device2", "uuid2"));

  const chromeos::disks::DiskMountManager::MountPointInfo kMountPoint1(
      "removable_device1", "/tmp/removable_device1",
      chromeos::MOUNT_TYPE_DEVICE, chromeos::disks::MOUNT_CONDITION_NONE);
  const chromeos::disks::DiskMountManager::MountPointInfo kMountPoint2(
      "removable_device2", "/tmp/removable_device2",
      chromeos::MOUNT_TYPE_DEVICE, chromeos::disks::MOUNT_CONDITION_NONE);

  // Mount |disk1|.
  mounted_disk_monitor_->OnMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      kMountPoint1,
      disk1.get());
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk1));

  // Pseudo system suspend and resume.
  mounted_disk_monitor_->SuspendImminent();
  mounted_disk_monitor_->SuspendDone(base::TimeDelta::FromSeconds(0));

  // On system resume, we expect unmount and then mount immediately.
  // During the phase, we expect the disk is remounting.
  mounted_disk_monitor_->OnMountEvent(
      chromeos::disks::DiskMountManager::UNMOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      kMountPoint1,
      disk1.get());
  EXPECT_TRUE(mounted_disk_monitor_->DiskIsRemounting(*disk1));

  mounted_disk_monitor_->OnMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      kMountPoint1,
      disk1.get());
  EXPECT_TRUE(mounted_disk_monitor_->DiskIsRemounting(*disk1));

  // New disk should not be "remounting."
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk2));
  mounted_disk_monitor_->OnMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      kMountPoint2,
      disk2.get());
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk2));

  // After certain period, remounting state should be cleared.
  base::RunLoop().RunUntilIdle();  // Emulate time passage.
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk1));
  EXPECT_FALSE(mounted_disk_monitor_->DiskIsRemounting(*disk2));
}

}  // namespace file_manager
