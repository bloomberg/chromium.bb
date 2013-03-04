// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DISKS_MOCK_DISK_MOUNT_MANAGER_H_
#define CHROMEOS_DISKS_MOCK_DISK_MOUNT_MANAGER_H_

#include <string>

#include "base/observer_list.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace disks {

class MockDiskMountManager : public DiskMountManager {
 public:
  MockDiskMountManager();
  virtual ~MockDiskMountManager();

  // DiskMountManager override.
  MOCK_METHOD0(Init, void(void));
  MOCK_METHOD1(AddObserver, void(DiskMountManager::Observer*));
  MOCK_METHOD1(RemoveObserver, void(DiskMountManager::Observer*));
  MOCK_CONST_METHOD0(disks, const DiskMountManager::DiskMap&(void));
  MOCK_CONST_METHOD1(FindDiskBySourcePath,
                     const DiskMountManager::Disk*(const std::string&));
  MOCK_CONST_METHOD0(mount_points,
                     const DiskMountManager::MountPointMap&(void));
  MOCK_METHOD0(RequestMountInfoRefresh, void(void));
  MOCK_METHOD4(MountPath, void(const std::string&, const std::string&,
                               const std::string&, MountType));
  MOCK_METHOD2(UnmountPath, void(const std::string&, UnmountOptions));
  MOCK_METHOD1(FormatMountedDevice, void(const std::string&));
  MOCK_METHOD2(
      UnmountDeviceRecursively,
      void(const std::string&,
           const DiskMountManager::UnmountDeviceRecursivelyCallbackType&));

  // Invokes fake device insert events.
  void NotifyDeviceInsertEvents();

  // Invokes fake device remove events.
  void NotifyDeviceRemoveEvents();

  // Sets up default results for mock methods.
  void SetupDefaultReplies();

  // Creates a fake disk entry for the mounted device. This function is
  // primarily for StorageMonitorTest.
  void CreateDiskEntryForMountDevice(
      const DiskMountManager::MountPointInfo& mount_info,
      const std::string& device_id,
      const std::string& device_label,
      const std::string& vendor_name,
      const std::string& product_name,
      DeviceType device_type,
      uint64 total_size_in_bytes);

  // Removes the fake disk entry associated with the mounted device. This
  // function is primarily for StorageMonitorTest.
  void RemoveDiskEntryForMountDevice(
      const DiskMountManager::MountPointInfo& mount_info);

 private:
  // Is used to implement AddObserver.
  void AddObserverInternal(DiskMountManager::Observer* observer);

  // Is used to implement RemoveObserver.
  void RemoveObserverInternal(DiskMountManager::Observer* observer);

  // Is used to implement disks.
  const DiskMountManager::DiskMap& disksInternal() const { return disks_; }

  const DiskMountManager::MountPointMap& mountPointsInternal() const;

  // Returns Disk object associated with the |source_path| or NULL on failure.
  const DiskMountManager::Disk* FindDiskBySourcePathInternal(
      const std::string& source_path) const;

  // Notifies observers about device status update.
  void NotifyDeviceChanged(DeviceEvent event,
                           const std::string& path);

  // Notifies observers about disk status update.
  void NotifyDiskChanged(DiskEvent event,
                         const DiskMountManager::Disk* disk);

  // The list of observers.
  ObserverList<DiskMountManager::Observer> observers_;

  // The list of disks found.
  DiskMountManager::DiskMap disks_;

  // The list of existing mount points.
  DiskMountManager::MountPointMap mount_points_;

  DISALLOW_COPY_AND_ASSIGN(MockDiskMountManager);
};

}  // namespace disks
}  // namespace chromeos

#endif  // CHROMEOS_DISKS_MOCK_DISK_MOUNT_MANAGER_H_
