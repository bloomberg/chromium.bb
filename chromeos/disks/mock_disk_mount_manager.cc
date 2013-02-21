// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/disks/mock_disk_mount_manager.h"

#include <utility>

#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_util.h"

using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::ReturnRef;

namespace chromeos {
namespace disks {

namespace {

const char* kTestSystemPath = "/this/system/path";
const char* kTestSystemPathPrefix = "/this/system";
const char* kTestDevicePath = "/this/device/path";
const char* kTestMountPath = "/media/foofoo";
const char* kTestFilePath = "/this/file/path";
const char* kTestDeviceLabel = "A label";
const char* kTestDriveLabel = "Another label";
const char* kTestVendorId = "0123";
const char* kTestVendorName = "A vendor";
const char* kTestProductId = "abcd";
const char* kTestProductName = "A product";
const char* kTestUuid = "FFFF-FFFF";

}  // namespace

void MockDiskMountManager::AddObserverInternal(
    DiskMountManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void MockDiskMountManager::RemoveObserverInternal(
    DiskMountManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

MockDiskMountManager::MockDiskMountManager() {
  ON_CALL(*this, AddObserver(_))
      .WillByDefault(Invoke(this, &MockDiskMountManager::AddObserverInternal));
  ON_CALL(*this, RemoveObserver(_))
      .WillByDefault(Invoke(this,
                            &MockDiskMountManager::RemoveObserverInternal));
  ON_CALL(*this, disks())
      .WillByDefault(Invoke(this, &MockDiskMountManager::disksInternal));
  ON_CALL(*this, mount_points())
      .WillByDefault(Invoke(this, &MockDiskMountManager::mountPointsInternal));
  ON_CALL(*this, FindDiskBySourcePath(_))
      .WillByDefault(Invoke(
          this, &MockDiskMountManager::FindDiskBySourcePathInternal));
}

MockDiskMountManager::~MockDiskMountManager() {
  STLDeleteContainerPairSecondPointers(disks_.begin(), disks_.end());
  disks_.clear();
}

void MockDiskMountManager::NotifyDeviceInsertEvents() {
  scoped_ptr<DiskMountManager::Disk> disk1(new DiskMountManager::Disk(
      std::string(kTestDevicePath),
      std::string(),
      std::string(kTestSystemPath),
      std::string(kTestFilePath),
      std::string(),
      std::string(kTestDriveLabel),
      std::string(kTestVendorId),
      std::string(kTestVendorName),
      std::string(kTestProductId),
      std::string(kTestProductName),
      std::string(kTestUuid),
      std::string(kTestSystemPathPrefix),
      DEVICE_TYPE_USB,
      4294967295U,
      false,  // is_parent
      false,  // is_read_only
      true,  // has_media
      false,  // on_boot_device
      false));  // is_hidden

  disks_.clear();
  disks_.insert(std::pair<std::string, DiskMountManager::Disk*>(
      std::string(kTestDevicePath), disk1.get()));

  // Device Added
  NotifyDeviceChanged(DEVICE_ADDED, kTestSystemPath);

  // Disk Added
  NotifyDiskChanged(DISK_ADDED, disk1.get());

  // Disk Changed
  scoped_ptr<DiskMountManager::Disk> disk2(new DiskMountManager::Disk(
      std::string(kTestDevicePath),
      std::string(kTestMountPath),
      std::string(kTestSystemPath),
      std::string(kTestFilePath),
      std::string(kTestDeviceLabel),
      std::string(kTestDriveLabel),
      std::string(kTestVendorId),
      std::string(kTestVendorName),
      std::string(kTestProductId),
      std::string(kTestProductName),
      std::string(kTestUuid),
      std::string(kTestSystemPathPrefix),
      DEVICE_TYPE_MOBILE,
      1073741824,
      false,  // is_parent
      false,  // is_read_only
      true,  // has_media
      false,  // on_boot_device
      false));  // is_hidden
  disks_.clear();
  disks_.insert(std::pair<std::string, DiskMountManager::Disk*>(
      std::string(kTestDevicePath), disk2.get()));
  NotifyDiskChanged(DISK_CHANGED, disk2.get());
}

void MockDiskMountManager::NotifyDeviceRemoveEvents() {
  scoped_ptr<DiskMountManager::Disk> disk(new DiskMountManager::Disk(
      std::string(kTestDevicePath),
      std::string(kTestMountPath),
      std::string(kTestSystemPath),
      std::string(kTestFilePath),
      std::string(kTestDeviceLabel),
      std::string(kTestDriveLabel),
      std::string(kTestVendorId),
      std::string(kTestVendorName),
      std::string(kTestProductId),
      std::string(kTestProductName),
      std::string(kTestUuid),
      std::string(kTestSystemPathPrefix),
      DEVICE_TYPE_SD,
      1073741824,
      false,  // is_parent
      false,  // is_read_only
      true,  // has_media
      false,  // on_boot_device
      false));  // is_hidden
  disks_.clear();
  disks_.insert(std::pair<std::string, DiskMountManager::Disk*>(
      std::string(kTestDevicePath), disk.get()));
  NotifyDiskChanged(DISK_REMOVED, disk.get());
}

void MockDiskMountManager::SetupDefaultReplies() {
  EXPECT_CALL(*this, AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*this, RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*this, disks())
      .WillRepeatedly(ReturnRef(disks_));
  EXPECT_CALL(*this, mount_points())
      .WillRepeatedly(ReturnRef(mount_points_));
  EXPECT_CALL(*this, FindDiskBySourcePath(_))
      .Times(AnyNumber());
  EXPECT_CALL(*this, RequestMountInfoRefresh())
      .Times(AnyNumber());
  EXPECT_CALL(*this, MountPath(_, _, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(*this, UnmountPath(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*this, FormatMountedDevice(_))
      .Times(AnyNumber());
  EXPECT_CALL(*this, UnmountDeviceRecursively(_, _))
      .Times(AnyNumber());
}

void MockDiskMountManager::CreateDiskEntryForMountDevice(
    const DiskMountManager::MountPointInfo& mount_info,
    const std::string& device_id,
    const std::string& device_label,
    const std::string& vendor_name,
    const std::string& product_name,
    DeviceType device_type,
    uint64 total_size_in_bytes) {
  Disk* disk = new DiskMountManager::Disk(std::string(mount_info.source_path),
                                          std::string(mount_info.mount_path),
                                          std::string(),  // system_path
                                          std::string(),  // file_path
                                          device_label,  // device_label
                                          std::string(),  // drive_label
                                          std::string(),  // vendor_id
                                          vendor_name,
                                          std::string(),  // product_id
                                          product_name,
                                          device_id,  // fs_uuid
                                          std::string(),  // system_path_prefix
                                          device_type,
                                          total_size_in_bytes,
                                          false,  // is_parent
                                          false,  // is_read_only
                                          true,  // has_media
                                          false,  // on_boot_device
                                          false);  // is_hidden
  DiskMountManager::DiskMap::iterator it = disks_.find(mount_info.source_path);
  if (it == disks_.end()) {
    disks_.insert(std::make_pair(std::string(mount_info.source_path), disk));
  } else {
    delete it->second;
    it->second = disk;
  }
}

void MockDiskMountManager::RemoveDiskEntryForMountDevice(
    const DiskMountManager::MountPointInfo& mount_info) {
  DiskMountManager::DiskMap::iterator it = disks_.find(mount_info.source_path);
  if (it != disks_.end()) {
    delete it->second;
    disks_.erase(it);
  }
}

const DiskMountManager::MountPointMap&
MockDiskMountManager::mountPointsInternal() const {
  return mount_points_;
}

const DiskMountManager::Disk*
MockDiskMountManager::FindDiskBySourcePathInternal(
    const std::string& source_path) const {
  DiskMap::const_iterator disk_it = disks_.find(source_path);
  return disk_it == disks_.end() ? NULL : disk_it->second;
}

void MockDiskMountManager::NotifyDiskChanged(
    DiskEvent event,
    const DiskMountManager::Disk* disk) {
  FOR_EACH_OBSERVER(Observer, observers_, OnDiskEvent(event, disk));
}

void MockDiskMountManager::NotifyDeviceChanged(DeviceEvent event,
                                               const std::string& path) {
  FOR_EACH_OBSERVER(Observer, observers_, OnDeviceEvent(event, path));
}

}  // namespace disks
}  // namespace chromeos
