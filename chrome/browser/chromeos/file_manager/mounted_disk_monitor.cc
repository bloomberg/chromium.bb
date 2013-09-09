// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/mounted_disk_monitor.h"

#include "base/bind.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/browser_thread.h"

using chromeos::disks::DiskMountManager;

namespace file_manager {
namespace {

// Time span of the resuming process. All unmount events sent during this
// time are considered as being part of remounting process, since remounting
// is done just after resuming.
const base::TimeDelta kResumingTimeSpan = base::TimeDelta::FromSeconds(5);

}  // namespace

MountedDiskMonitor::MountedDiskMonitor(
    chromeos::PowerManagerClient* power_manager_client,
    chromeos::disks::DiskMountManager* disk_mount_manager)
    : power_manager_client_(power_manager_client),
      disk_mount_manager_(disk_mount_manager),
      is_resuming_(false),
      resuming_time_span_(kResumingTimeSpan),
      weak_factory_(this) {
  DCHECK(power_manager_client_);
  DCHECK(disk_mount_manager_);
  power_manager_client_->AddObserver(this);
  disk_mount_manager_->AddObserver(this);
  disk_mount_manager_->RequestMountInfoRefresh();
}

MountedDiskMonitor::~MountedDiskMonitor() {
  disk_mount_manager_->RemoveObserver(this);
  power_manager_client_->RemoveObserver(this);
}

void MountedDiskMonitor::SuspendImminent() {
  is_resuming_ = false;
  weak_factory_.InvalidateWeakPtrs();
}

void MountedDiskMonitor::SystemResumed(
    const base::TimeDelta& sleep_duration) {
  is_resuming_ = true;
  // Undo any previous resets.
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

void MountedDiskMonitor::OnMountEvent(
    chromeos::disks::DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const chromeos::disks::DiskMountManager::MountPointInfo& mount_info) {
  if (mount_info.mount_type != chromeos::MOUNT_TYPE_DEVICE)
    return;

  switch (event) {
    case DiskMountManager::MOUNTING: {
      const DiskMountManager::Disk* disk =
          disk_mount_manager_->FindDiskBySourcePath(mount_info.source_path);
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
}

void MountedDiskMonitor::OnDeviceEvent(
    chromeos::disks::DiskMountManager::DeviceEvent event,
    const std::string& device_path) {
}

void MountedDiskMonitor::OnFormatEvent(
    chromeos::disks::DiskMountManager::FormatEvent event,
    chromeos::FormatError error_code,
    const std::string& device_path) {
}

void MountedDiskMonitor::Reset() {
  unmounted_while_resuming_.clear();
  is_resuming_ = false;
}

}  // namespace file_manager
