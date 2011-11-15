// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISKS_MOCK_DISK_MOUNT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_DISKS_MOCK_DISK_MOUNT_MANAGER_H_
#pragma once

#include <string>

#include "base/observer_list.h"
#include "base/time.h"
#include "chrome/browser/chromeos/disks/disk_mount_manager.h"
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
  MOCK_CONST_METHOD0(mount_points,
                     const DiskMountManager::MountPointMap&(void));
  MOCK_METHOD0(RequestMountInfoRefresh, void(void));
  MOCK_METHOD2(MountPath, void(const std::string&, MountType));
  MOCK_METHOD1(UnmountPath, void(const std::string&));
  MOCK_METHOD3(GetSizeStatsOnFileThread, void(const std::string&, size_t*,
                                              size_t*));
  MOCK_METHOD1(FormatUnmountedDevice, void(const std::string&));
  MOCK_METHOD1(FormatMountedDevice, void(const std::string&));
  MOCK_METHOD3(UnmountDeviceRecursive, void(const std::string&,
      DiskMountManager::UnmountDeviceRecursiveCallbackType, void*));

  // Invokes fake device insert events.
  void NotifyDeviceInsertEvents();

  // Invokes fake device remove events.
  void NotifyDeviceRemoveEvents();

  // Sets up default results for mock methods.
  void SetupDefaultReplies();

 private:
  // Is used to implement AddObserver.
  void AddObserverInternal(DiskMountManager::Observer* observer);

  // Is used to implement RemoveObserver.
  void RemoveObserverInternal(DiskMountManager::Observer* observer);

  // Is used to implement disks.
  const DiskMountManager::DiskMap& disksInternal() const { return disks_; }

  // Notifies observers about device status update.
  void NotifyDeviceChanged(DiskMountManagerEventType event,
                           const std::string& path);

  // Notifies observers about disk status update.
  void NotifyDiskChanged(DiskMountManagerEventType event,
                         const DiskMountManager::Disk* disk);

  // The list of observers.
  ObserverList<DiskMountManager::Observer> observers_;

  // The list of disks found.
  DiskMountManager::DiskMap disks_;

  DISALLOW_COPY_AND_ASSIGN(MockDiskMountManager);
};

}  // namespace disks
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DISKS_MOCK_DISK_MOUNT_MANAGER_H_
