// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "chromeos/disks/suspend_unmount_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace disks {
namespace {

class FakeDiskMountManager : public MockDiskMountManager {
 public:
  void NotifyUnmountDeviceComplete(MountError error) const {
    callback_.Run(error);
  }

  const std::vector<std::string>& unmounting_mount_paths() const {
    return unmounting_mount_paths_;
  }

 private:
  void UnmountPath(const std::string& mount_path,
                   UnmountOptions options,
                   const UnmountPathCallback& callback) override {
    unmounting_mount_paths_.push_back(mount_path);
    callback_ = callback;
  }
  std::vector<std::string> unmounting_mount_paths_;
  UnmountPathCallback callback_;
};

class SuspendUnmountManagerTest : public testing::Test {
 public:
  SuspendUnmountManagerTest()
      : suspend_unmount_manager_(&disk_mount_manager_, &fake_power_client_) {}
  ~SuspendUnmountManagerTest() override {}

 protected:
  FakeDiskMountManager disk_mount_manager_;
  FakePowerManagerClient fake_power_client_;
  SuspendUnmountManager suspend_unmount_manager_;
};

TEST_F(SuspendUnmountManagerTest, Basic) {
  const std::string dummy_mount_path = "/dummy/mount";
  disk_mount_manager_.CreateDiskEntryForMountDevice(
      chromeos::disks::DiskMountManager::MountPointInfo(
          "/dummy/device", dummy_mount_path, chromeos::MOUNT_TYPE_DEVICE,
          chromeos::disks::MOUNT_CONDITION_NONE),
      "device_id", "device_label", "Vendor", "Product",
      chromeos::DEVICE_TYPE_USB, 1024 * 1024, true, true, true, false);
  disk_mount_manager_.SetupDefaultReplies();
  fake_power_client_.SendSuspendImminent();

  EXPECT_EQ(1, fake_power_client_.GetNumPendingSuspendReadinessCallbacks());
  ASSERT_EQ(1u, disk_mount_manager_.unmounting_mount_paths().size());
  EXPECT_EQ(dummy_mount_path, disk_mount_manager_.unmounting_mount_paths()[0]);
  disk_mount_manager_.NotifyUnmountDeviceComplete(MOUNT_ERROR_NONE);
  EXPECT_EQ(0, fake_power_client_.GetNumPendingSuspendReadinessCallbacks());
}

TEST_F(SuspendUnmountManagerTest, CancelAndSuspendAgain) {
  const std::string dummy_mount_path = "/dummy/mount";
  disk_mount_manager_.CreateDiskEntryForMountDevice(
      chromeos::disks::DiskMountManager::MountPointInfo(
          "/dummy/device", dummy_mount_path, chromeos::MOUNT_TYPE_DEVICE,
          chromeos::disks::MOUNT_CONDITION_NONE),
      "device_id", "device_label", "Vendor", "Product",
      chromeos::DEVICE_TYPE_USB, 1024 * 1024, true, true, true, false);
  disk_mount_manager_.SetupDefaultReplies();
  fake_power_client_.SendSuspendImminent();
  EXPECT_EQ(1, fake_power_client_.GetNumPendingSuspendReadinessCallbacks());
  ASSERT_EQ(1u, disk_mount_manager_.unmounting_mount_paths().size());
  EXPECT_EQ(dummy_mount_path, disk_mount_manager_.unmounting_mount_paths()[0]);

  // Suspend cancelled.
  fake_power_client_.SendSuspendDone();

  // Suspend again.
  fake_power_client_.SendSuspendImminent();
  ASSERT_EQ(2u, disk_mount_manager_.unmounting_mount_paths().size());
  EXPECT_EQ(dummy_mount_path, disk_mount_manager_.unmounting_mount_paths()[1]);
}

}  // namespace
}  // namespace chromeos
}  // namespace disks
