// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/imageburner/burn_device_handler.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/disks/disk_mount_manager.h"

namespace chromeos {
namespace imageburner {

using disks::DiskMountManager;

namespace {

// Returns true when |disk| is a device on which we can burn recovery image.
bool IsBurnableDevice(const DiskMountManager::Disk& disk) {
  return disk.is_parent() && !disk.on_boot_device() && disk.has_media() &&
         (disk.device_type() == DEVICE_TYPE_USB ||
          disk.device_type() == DEVICE_TYPE_SD);
}

}  // namespace

BurnDeviceHandler::BurnDeviceHandler(DiskMountManager* disk_mount_manager)
    : disk_mount_manager_(disk_mount_manager) {
  DCHECK(disk_mount_manager_);
  disk_mount_manager_->AddObserver(this);
}

BurnDeviceHandler::~BurnDeviceHandler() {
  disk_mount_manager_->RemoveObserver(this);
}

void BurnDeviceHandler::SetCallbacks(const DiskCallback& add_callback,
                                     const DiskCallback& remove_callback) {
  add_callback_ = add_callback;
  remove_callback_ = remove_callback;
}

std::vector<DiskMountManager::Disk> BurnDeviceHandler::GetBurnableDevices() {
  const DiskMountManager::DiskMap& disks = disk_mount_manager_->disks();
  std::vector<DiskMountManager::Disk> result;
  for (DiskMountManager::DiskMap::const_iterator iter = disks.begin();
       iter != disks.end();
       ++iter) {
    const DiskMountManager::Disk& disk = *iter->second;
    if (IsBurnableDevice(disk))
      result.push_back(disk);
  }
  return result;
}

void BurnDeviceHandler::OnDiskEvent(DiskMountManager::DiskEvent event,
                                    const DiskMountManager::Disk* disk) {
  // We are only interested in burnable devices.
  if (!IsBurnableDevice(*disk))
    return;

  switch (event) {
    case DiskMountManager::DISK_ADDED:
      add_callback_.Run(*disk);
      break;
    case DiskMountManager::DISK_REMOVED:
      remove_callback_.Run(*disk);
      break;
    default: {
      // Do nothing.
    }
  }
}

void BurnDeviceHandler::OnDeviceEvent(DiskMountManager::DeviceEvent event,
                                      const std::string& device_path) {
  // Do nothing.
}

void BurnDeviceHandler::OnMountEvent(
    DiskMountManager::MountEvent event,
    MountError error_code,
    const DiskMountManager::MountPointInfo& mount_info) {
  // Do nothing.
}

void BurnDeviceHandler::OnFormatEvent(DiskMountManager::FormatEvent event,
                                      FormatError error_code,
                                      const std::string& device_path) {
  // Do nothing.
}

}  // namespace imageburner
}  // namespace chromeos
