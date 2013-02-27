// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_IMAGEBURNER_BURN_DEVICE_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_IMAGEBURNER_BURN_DEVICE_HANDLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/disks/disk_mount_manager.h"

namespace chromeos {
namespace imageburner {

// This is the implementation for the communication between BurnManager
// and DiskMountManager.
// The main reason this is NOT merged into BurnManager is to improve
// testability, since both BurnManager and DiskMountManager are singleton
// in real usage.
class BurnDeviceHandler : public disks::DiskMountManager::Observer {
 public:
  // Triggered when a burnable device is added or removed.
  typedef base::Callback<void(const disks::DiskMountManager::Disk& disk)>
      DiskCallback;

  // This class takes the pointer of DiskMountManager to improve testability,
  // although it is singleton in the real usage.
  explicit BurnDeviceHandler(disks::DiskMountManager* disk_mount_manager);
  virtual ~BurnDeviceHandler();

  // |add_callback| will be called when a new burnable device is added with
  // the device's information.
  // |remove_callback| will be called when a burnable device is removed.
  // Note: This class is designed to connect to only one BurnManager,
  // so it supports only single callback for each add and remove intentionally
  // (rather than ObserverList).
  void SetCallbacks(const DiskCallback& add_callback,
                    const DiskCallback& remove_callback);

  // Returns devices on which we can burn recovery image.
  std::vector<disks::DiskMountManager::Disk> GetBurnableDevices();

  // DiskMountManager::Observer overrides.
  virtual void OnDiskEvent(
      disks::DiskMountManager::DiskEvent event,
      const disks::DiskMountManager::Disk* disk) OVERRIDE;
  virtual void OnDeviceEvent(
      disks::DiskMountManager::DeviceEvent event,
      const std::string& device_path) OVERRIDE;
  virtual void OnMountEvent(
      disks::DiskMountManager::MountEvent event,
      MountError error_code,
      const disks::DiskMountManager::MountPointInfo& mount_info) OVERRIDE;
  virtual void OnFormatEvent(
      disks::DiskMountManager::FormatEvent event,
      FormatError error_code,
      const std::string& device_path) OVERRIDE;

 private:
  disks::DiskMountManager* disk_mount_manager_;  // Not owned by this class.
  DiskCallback add_callback_;
  DiskCallback remove_callback_;

  DISALLOW_COPY_AND_ASSIGN(BurnDeviceHandler);
};

}  // namespace imageburner
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_IMAGEBURNER_BURN_DEVICE_HANDLER_H_
