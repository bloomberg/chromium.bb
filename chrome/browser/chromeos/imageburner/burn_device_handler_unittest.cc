// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/imageburner/burn_device_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace imageburner {

namespace {

const bool kIsParent = true;
const bool kIsBootDevice = true;
const bool kHasMedia = true;

class FakeDiskMountManager : public disks::DiskMountManager {
 public:
  FakeDiskMountManager() {}

  virtual ~FakeDiskMountManager() {
    STLDeleteValues(&disks_);
  }

  // Emulates to add new disk physically (e.g., connecting a
  // new USB flash to a Chrome OS).
  void EmulateAddDisk(scoped_ptr<Disk> in_disk) {
    DCHECK(in_disk.get());
    // Keep the reference for the callback, before passing the ownership to
    // InsertDisk. It should be safe, because it won't be deleted in
    // InsertDisk.
    Disk* disk = in_disk.get();
    bool new_disk = InsertDisk(disk->device_path(), in_disk.Pass());
    FOR_EACH_OBSERVER(
        Observer, observers_,
        OnDiskEvent(new_disk ? DISK_ADDED : DISK_CHANGED, disk));
  }

  // Emulates to remove a disk phyically (e.g., removing a USB flash from
  // a Chrome OS).
  void EmulateRemoveDisk(const std::string& source_path) {
    scoped_ptr<Disk> disk(RemoveDisk(source_path));
    if (disk.get()) {
      FOR_EACH_OBSERVER(
          Observer, observers_, OnDiskEvent(DISK_REMOVED, disk.get()));
    }
  }

  // DiskMountManager overrides.
  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual const DiskMap& disks() const OVERRIDE {
    return disks_;
  }

  // Following methods are not implemented.
  virtual const Disk* FindDiskBySourcePath(
      const std::string& source_path) const OVERRIDE {
    return NULL;
  }
  virtual const MountPointMap& mount_points() const OVERRIDE {
    // Note: mount_points_ will always be empty, now.
    return mount_points_;
  }
  virtual void RequestMountInfoRefresh() OVERRIDE {}
  virtual void MountPath(const std::string& source_path,
                         const std::string& source_format,
                         const std::string& mount_label,
                         MountType type) OVERRIDE {}
  virtual void UnmountPath(const std::string& mount_path,
                           UnmountOptions options) OVERRIDE {}
  virtual void FormatMountedDevice(const std::string& mount_path) OVERRIDE {}
  virtual void UnmountDeviceRecursively(
      const std::string& device_path,
      const UnmountDeviceRecursivelyCallbackType& callback) OVERRIDE {}
  virtual bool AddDiskForTest(Disk* disk) OVERRIDE { return false; }
  virtual bool AddMountPointForTest(
      const MountPointInfo& mount_point) OVERRIDE {
    return false;
  }

 private:
  bool InsertDisk(const std::string& path, scoped_ptr<Disk> disk) {
    std::pair<DiskMap::iterator, bool> insert_result =
        disks_.insert(std::pair<std::string, Disk*>(path, NULL));
    if (!insert_result.second) {
      // There is already an entry. Delete it before replacing.
      delete insert_result.first->second;
    }
    insert_result.first->second = disk.release();  // Moves ownership.
    return insert_result.second;
  }

  scoped_ptr<Disk> RemoveDisk(const std::string& path) {
    DiskMap::iterator iter = disks_.find(path);
    if (iter == disks_.end()) {
      // Not found.
      return scoped_ptr<Disk>();
    }
    scoped_ptr<Disk> result(iter->second);
    disks_.erase(iter);
    return result.Pass();
  }

  ObserverList<Observer> observers_;
  DiskMap disks_;
  MountPointMap mount_points_;

  DISALLOW_COPY_AND_ASSIGN(FakeDiskMountManager);
};

void CopyDevicePathCallback(
    std::string* out_path, const disks::DiskMountManager::Disk& disk) {
  *out_path = disk.device_path();
}

}  // namespace

class BurnDeviceHandlerTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    disk_mount_manager_.reset(new FakeDiskMountManager);
  }

  virtual void TearDown() OVERRIDE {
    disk_mount_manager_.reset();
  }

  static scoped_ptr<disks::DiskMountManager::Disk> CreateMockDisk(
      const std::string& device_path,
      bool is_parent,
      bool on_boot_device,
      bool has_media,
      DeviceType device_type) {
    return scoped_ptr<disks::DiskMountManager::Disk>(
        new disks::DiskMountManager::Disk(
            device_path,
            "",  // mount path
            "",  // system_path
            "",  // file_path
            "",  // device label
            "",  // drive label
            "",  // vendor id
            "",  // vendor name
            "",  // product id
            "",  // product name
            "",  // fs uuid
            "",  // system path prefix
            device_type,
            0,  // total size in bytes
            is_parent,
            false,  //  is read only
            has_media,
            on_boot_device,
            false));  // is hidden
  }

  scoped_ptr<FakeDiskMountManager> disk_mount_manager_;
};

TEST_F(BurnDeviceHandlerTest, GetBurnableDevices) {
  // The devices which should be retrieved as burnable.
  disk_mount_manager_->EmulateAddDisk(
      CreateMockDisk("/dev/burnable_usb",
                     kIsParent, !kIsBootDevice, kHasMedia, DEVICE_TYPE_USB));
  disk_mount_manager_->EmulateAddDisk(
      CreateMockDisk("/dev/burnable_sd",
                     kIsParent, !kIsBootDevice, kHasMedia, DEVICE_TYPE_SD));

  // If the device type is neither USB nor SD, it shouldn't be burnable.
  disk_mount_manager_->EmulateAddDisk(
      CreateMockDisk(
          "/dev/non_burnable_unknown",
          kIsParent, !kIsBootDevice, kHasMedia, DEVICE_TYPE_UNKNOWN));
  disk_mount_manager_->EmulateAddDisk(
      CreateMockDisk("/dev/non_burnable_dvd",
                     kIsParent, !kIsBootDevice, kHasMedia, DEVICE_TYPE_DVD));

  // If not parent, it shouldn't be burnable.
  disk_mount_manager_->EmulateAddDisk(
      CreateMockDisk("/dev/non_burnable_not_parent",
                     !kIsParent, !kIsBootDevice, kHasMedia, DEVICE_TYPE_USB));

  // If on_boot_device, it shouldn't be burnable.
  disk_mount_manager_->EmulateAddDisk(
      CreateMockDisk("/dev/non_burnable_boot_device",
                     kIsParent, kIsBootDevice, kHasMedia, DEVICE_TYPE_USB));

  // If no media, it shouldn't be burnable.
  disk_mount_manager_->EmulateAddDisk(
      CreateMockDisk("/dev/non_burnable_no_media",
                     kIsParent, !kIsBootDevice, !kHasMedia, DEVICE_TYPE_USB));

  BurnDeviceHandler handler(disk_mount_manager_.get());

  const std::vector<disks::DiskMountManager::Disk>& burnable_devices =
      handler.GetBurnableDevices();
  ASSERT_EQ(2u, burnable_devices.size());
  bool burnable_usb_found = false;
  bool burnable_sd_found = false;
  for (size_t i = 0; i < burnable_devices.size(); ++i) {
    const std::string& device_path = burnable_devices[i].device_path();
    burnable_usb_found |= (device_path == "/dev/burnable_usb");
    burnable_sd_found |= (device_path == "/dev/burnable_sd");
  }

  EXPECT_TRUE(burnable_usb_found);
  EXPECT_TRUE(burnable_sd_found);
}

TEST_F(BurnDeviceHandlerTest, Callback) {
  std::string added_device;
  std::string removed_device;

  BurnDeviceHandler handler(disk_mount_manager_.get());
  handler.SetCallbacks(
      base::Bind(CopyDevicePathCallback, &added_device),
      base::Bind(CopyDevicePathCallback, &removed_device));

  // Emulate to connect a burnable device.
  // |add_callback| should be invoked.
  disk_mount_manager_->EmulateAddDisk(
      CreateMockDisk("/dev/burnable",
                     kIsParent, !kIsBootDevice, kHasMedia, DEVICE_TYPE_USB));
  EXPECT_EQ("/dev/burnable", added_device);
  EXPECT_TRUE(removed_device.empty());

  // Emulate to change the currently connected burnable device.
  // Neither |add_callback| nor |remove_callback| should be called.
  added_device.clear();
  removed_device.clear();
  disk_mount_manager_->EmulateAddDisk(
      CreateMockDisk("/dev/burnable",
                     kIsParent, !kIsBootDevice, kHasMedia, DEVICE_TYPE_USB));
  EXPECT_TRUE(added_device.empty());
  EXPECT_TRUE(removed_device.empty());

  // Emulate to disconnect the burnable device.
  // |remove_callback| should be called.
  added_device.clear();
  removed_device.clear();
  disk_mount_manager_->EmulateRemoveDisk("/dev/burnable");
  EXPECT_TRUE(added_device.empty());
  EXPECT_EQ("/dev/burnable", removed_device);

  // Emulate to connect and unconnect an unburnable device.
  // For each case, neither |add_callback| nor |remove_callback| should be
  // called.
  added_device.clear();
  removed_device.clear();
  disk_mount_manager_->EmulateAddDisk(
      CreateMockDisk("/dev/unburnable",
                     !kIsParent, !kIsBootDevice, kHasMedia, DEVICE_TYPE_USB));
  EXPECT_TRUE(added_device.empty());
  EXPECT_TRUE(removed_device.empty());

  added_device.clear();
  removed_device.clear();
  disk_mount_manager_->EmulateRemoveDisk("/dev/unburnable");
  EXPECT_TRUE(added_device.empty());
  EXPECT_TRUE(removed_device.empty());
}

}  // namespace imageburner
}  // namespace chromeos
