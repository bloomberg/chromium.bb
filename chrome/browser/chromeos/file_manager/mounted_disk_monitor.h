// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_MOUNTED_DISK_MONITOR_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_MOUNTED_DISK_MONITOR_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/disks/disk_mount_manager.h"

namespace file_manager {

// Observes PowerManager and updates its state when the system suspends and
// resumes. After the system resumes it will stay in "is_resuming" state for
// couple of seconds. This is to give DiskManager time to process device
// removed/added events (events for the devices that were present before suspend
// should not trigger any new notifications or file manager windows).
class MountedDiskMonitor
    : public chromeos::PowerManagerClient::Observer,
      public chromeos::disks::DiskMountManager::Observer {
 public:
  MountedDiskMonitor();
  virtual ~MountedDiskMonitor();

  // PowerManagerClient::Observer overrides:
  virtual void SuspendImminent() OVERRIDE;
  virtual void SystemResumed(const base::TimeDelta& sleep_duration) OVERRIDE;

  // DiskMountManager::Observer overrides.
  virtual void OnDiskEvent(
      chromeos::disks::DiskMountManager::DiskEvent event,
      const chromeos::disks::DiskMountManager::Disk* disk) OVERRIDE;
  virtual void OnDeviceEvent(
      chromeos::disks::DiskMountManager::DeviceEvent event,
      const std::string& device_path) OVERRIDE;
  virtual void OnMountEvent(
      chromeos::disks::DiskMountManager::MountEvent event,
      chromeos::MountError error_code,
      const chromeos::disks::DiskMountManager::MountPointInfo& mount_info)
      OVERRIDE;
  virtual void OnFormatEvent(
      chromeos::disks::DiskMountManager::FormatEvent event,
      chromeos::FormatError error_code,
      const std::string& device_path) OVERRIDE;

  // Checks if the disk is being remounted. The disk is remounting if it has
  // been unmounted during the resuming time span.
  bool DiskIsRemounting(
      const chromeos::disks::DiskMountManager::Disk& disk) const;
 private:
  // Maps source paths with corresponding uuids.
  typedef std::map<std::string, std::string> DiskMap;

  // Set of uuids.
  typedef std::set<std::string> DiskSet;

  void Reset();

  bool is_resuming_;
  DiskMap mounted_disks_;
  DiskSet unmounted_while_resuming_;
  base::WeakPtrFactory<MountedDiskMonitor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MountedDiskMonitor);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_MOUNTED_DISK_MONITOR_H_
