// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FAKE_DISK_MOUNT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FAKE_DISK_MOUNT_MANAGER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/disks/disk_mount_manager.h"

namespace file_manager {

class FakeDiskMountManager : public chromeos::disks::DiskMountManager {
 public:
  struct MountRequest {
    MountRequest(const std::string& source_path,
                 const std::string& source_format,
                 const std::string& mount_label,
                 chromeos::MountType type);

    std::string source_path;
    std::string source_format;
    std::string mount_label;
    chromeos::MountType type;
  };

  struct UnmountRequest {
    UnmountRequest(const std::string& mount_path,
                   chromeos::UnmountOptions options);

    std::string mount_path;
    chromeos::UnmountOptions options;
  };

  FakeDiskMountManager();
  virtual ~FakeDiskMountManager();

  const std::vector<MountRequest>& mount_requests() const {
    return mount_requests_;
  }
  const std::vector<UnmountRequest>& unmount_requests() const {
    return unmount_requests_;
  }

  // DiskMountManager overrides.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual const DiskMap& disks() const OVERRIDE;
  virtual const Disk* FindDiskBySourcePath(
      const std::string& source_path) const OVERRIDE;
  virtual const MountPointMap& mount_points() const OVERRIDE;
  virtual void EnsureMountInfoRefreshed(
      const EnsureMountInfoRefreshedCallback& callback) OVERRIDE;
  virtual void MountPath(const std::string& source_path,
                         const std::string& source_format,
                         const std::string& mount_label,
                         chromeos::MountType type) OVERRIDE;
  virtual void UnmountPath(const std::string& mount_path,
                           chromeos::UnmountOptions options,
                           const UnmountPathCallback& callback) OVERRIDE;
  virtual void FormatMountedDevice(const std::string& mount_path) OVERRIDE;
  virtual void UnmountDeviceRecursively(
      const std::string& device_path,
      const UnmountDeviceRecursivelyCallbackType& callback) OVERRIDE;

  virtual bool AddDiskForTest(Disk* disk) OVERRIDE;
  virtual bool AddMountPointForTest(
      const MountPointInfo& mount_point) OVERRIDE;
  void InvokeDiskEventForTest(DiskEvent event, const Disk* disk);

 private:
  ObserverList<Observer> observers_;

  DiskMap disks_;
  MountPointMap mount_points_;

  std::vector<MountRequest> mount_requests_;
  std::vector<UnmountRequest> unmount_requests_;

  DISALLOW_COPY_AND_ASSIGN(FakeDiskMountManager);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FAKE_DISK_MOUNT_MANAGER_H_
