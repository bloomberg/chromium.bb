// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/mounted_disk_monitor.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chromeos/dbus/power_manager_client.h"

using chromeos::disks::DiskMountManager;

namespace file_manager {
namespace {

// Time span of the resuming process. All unmount events sent during this
// time are considered as being part of remounting process, since remounting
// is done just after resuming.
const base::TimeDelta kResumingTimeSpan = base::TimeDelta::FromSeconds(5);

}  // namespace

MountedDiskMonitor::MountedDiskMonitor(
    chromeos::PowerManagerClient* power_manager_client)
    : power_manager_client_(power_manager_client),
      is_resuming_(false),
      resuming_time_span_(kResumingTimeSpan),
      weak_factory_(this) {
  DCHECK(power_manager_client_);
  power_manager_client_->AddObserver(this);
}

MountedDiskMonitor::~MountedDiskMonitor() {
  power_manager_client_->RemoveObserver(this);
}

void MountedDiskMonitor::SuspendImminent() {
  // Flip the resuming flag while suspending, so it is possible to detect
  // resuming as soon as possible after the lid is open. Note, that mount
  // events may occur before the SuspendDone method is called.
  is_resuming_ = true;
  weak_factory_.InvalidateWeakPtrs();
}

void MountedDiskMonitor::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  // Undo any previous resets. Release the resuming flag after a fixed timeout.
  weak_factory_.InvalidateWeakPtrs();
  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MountedDiskMonitor::Reset,
                 weak_factory_.GetWeakPtr()),
      resuming_time_span_);
}

bool MountedDiskMonitor::DiskIsRemounting(
    const DiskMountManager::Disk& disk) const {
  return unmounted_while_resuming_.count(disk.fs_uuid()) > 0;
}

bool MountedDiskMonitor::DeviceIsHardUnpluggedButNotReported(
    const std::string& device_path) const {
  const std::map<std::string, HardUnpluggedState>::const_iterator it
      = hard_unplugged_.find(device_path);
  return it != hard_unplugged_.end() && it->second == HARD_UNPLUGGED;
}

void MountedDiskMonitor::MarkAsHardUnpluggedReported(
    const std::string& device_path) {
  hard_unplugged_[device_path] = HARD_UNPLUGGED_AND_REPORTED;
}

void MountedDiskMonitor::OnMountEvent(
    chromeos::disks::DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const chromeos::disks::DiskMountManager::MountPointInfo& mount_info,
    const DiskMountManager::Disk* disk) {
  if (mount_info.mount_type != chromeos::MOUNT_TYPE_DEVICE)
    return;

  switch (event) {
    case DiskMountManager::MOUNTING: {
      if (!disk || error_code != chromeos::MOUNT_ERROR_NONE)
        return;
      mounted_disks_[mount_info.source_path] = disk->fs_uuid();
      break;
    }

    case DiskMountManager::UNMOUNTING: {
      DiskMap::iterator it = mounted_disks_.find(mount_info.source_path);
      if (it == mounted_disks_.end())
        return;
      const std::string& fs_uuid = it->second;
      if (is_resuming_)
        unmounted_while_resuming_.insert(fs_uuid);
      mounted_disks_.erase(it);
      break;
    }
  }
}

void MountedDiskMonitor::OnDiskEvent(
    chromeos::disks::DiskMountManager::DiskEvent event,
    const chromeos::disks::DiskMountManager::Disk* disk) {
  if (event == chromeos::disks::DiskMountManager::DISK_REMOVED) {
    // If the mount path is not empty, the disk is hard unplugged.
    if (!is_resuming_ &&
        !disk->mount_path().empty() &&
        hard_unplugged_.find(disk->system_path_prefix()) ==
        hard_unplugged_.end()) {
      hard_unplugged_.insert(
          std::make_pair(disk->system_path_prefix(), HARD_UNPLUGGED));
    }
  }
}

void MountedDiskMonitor::OnDeviceEvent(
    chromeos::disks::DiskMountManager::DeviceEvent event,
    const std::string& device_path) {
  if (event == chromeos::disks::DiskMountManager::DEVICE_REMOVED) {
    const std::map<std::string, HardUnpluggedState>::iterator it
        = hard_unplugged_.find(device_path);
    if (it != hard_unplugged_.end())
      hard_unplugged_.erase(it);
  }
}

void MountedDiskMonitor::Reset() {
  unmounted_while_resuming_.clear();
  is_resuming_ = false;
}

}  // namespace file_manager
