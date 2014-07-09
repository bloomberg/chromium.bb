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
class MountedDiskMonitor : public chromeos::PowerManagerClient::Observer {
 public:
  explicit MountedDiskMonitor(
      chromeos::PowerManagerClient* power_manager_client);
  virtual ~MountedDiskMonitor();

  // PowerManagerClient::Observer overrides:
  virtual void SuspendImminent() OVERRIDE;
  virtual void SuspendDone(const base::TimeDelta& sleep_duration) OVERRIDE;

  // Receives forwarded notifications originates from DiskMountManager.
  void OnDiskEvent(
      chromeos::disks::DiskMountManager::DiskEvent event,
      const chromeos::disks::DiskMountManager::Disk* disk);
  void OnDeviceEvent(
      chromeos::disks::DiskMountManager::DeviceEvent event,
      const std::string& device_path);
  void OnMountEvent(
      chromeos::disks::DiskMountManager::MountEvent event,
      chromeos::MountError error_code,
      const chromeos::disks::DiskMountManager::MountPointInfo& mount_info,
      const chromeos::disks::DiskMountManager::Disk* disk);

  // Checks if the disk is being remounted. The disk is remounting if it has
  // been unmounted during the resuming time span.
  bool DiskIsRemounting(
      const chromeos::disks::DiskMountManager::Disk& disk) const;
  bool DeviceIsHardUnpluggedButNotReported(
      const std::string& device_path) const;
  void MarkAsHardUnpluggedReported(const std::string& device_path);

  // In order to avoid consuming time a lot for testing, this allows to set the
  // resuming time span.
  void set_resuming_time_span_for_testing(
      const base::TimeDelta& resuming_time_span) {
    resuming_time_span_ = resuming_time_span;
  }

 private:
  enum HardUnpluggedState { HARD_UNPLUGGED, HARD_UNPLUGGED_AND_REPORTED };
  // Maps source paths with corresponding uuids.
  typedef std::map<std::string, std::string> DiskMap;

  // Set of uuids.
  typedef std::set<std::string> DiskSet;

  void Reset();

  chromeos::PowerManagerClient* power_manager_client_;

  bool is_resuming_;
  DiskMap mounted_disks_;
  DiskSet unmounted_while_resuming_;
  // Set of device path that is hard unplugged.
  std::map<std::string, HardUnpluggedState> hard_unplugged_;
  base::TimeDelta resuming_time_span_;
  base::WeakPtrFactory<MountedDiskMonitor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MountedDiskMonitor);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_MOUNTED_DISK_MONITOR_H_
