// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chromeos::RemovableDeviceNotificationsCros implementation.

#include "chrome/browser/storage_monitor/removable_device_notifications_chromeos.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/media_device_notifications_utils.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/storage_monitor/removable_device_constants.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

// Constructs a device name using label or manufacturer (vendor and product)
// name details.
string16 GetDeviceName(const disks::DiskMountManager::Disk& disk) {
  if (disk.device_type() == DEVICE_TYPE_SD) {
    // Mount path of an SD card will be one of the following:
    // (1) /media/removable/<volume_label>
    // (2) /media/removable/SD Card
    // If the volume label is available, mount path will be (1) else (2).
    base::FilePath mount_point(disk.mount_path());
    const string16 display_name(mount_point.BaseName().LossyDisplayName());
    if (!display_name.empty())
      return display_name;
  }

  const std::string& device_label = disk.device_label();
  if (!device_label.empty() && IsStringUTF8(device_label))
    return UTF8ToUTF16(device_label);
  return chrome::GetFullProductName(disk.vendor_name(), disk.product_name());
}

// Constructs a device id using uuid or manufacturer (vendor and product) id
// details.
std::string MakeDeviceUniqueId(const disks::DiskMountManager::Disk& disk) {
  std::string uuid = disk.fs_uuid();
  if (!uuid.empty())
    return chrome::kFSUniqueIdPrefix + uuid;

  // If one of the vendor or product information is missing, its value in the
  // string is empty.
  // Format: VendorModelSerial:VendorInfo:ModelInfo:SerialInfo
  // TODO(kmadhusu) Extract serial information for the disks and append it to
  // the device unique id.
  const std::string& vendor = disk.vendor_id();
  const std::string& product = disk.product_id();
  if (vendor.empty() && product.empty())
    return std::string();
  return chrome::kVendorModelSerialPrefix + vendor + ":" + product + ":";
}

// Returns true if the requested device is valid, else false. On success, fills
// in |unique_id|, |device_label| and |storage_size_in_bytes|.
bool GetDeviceInfo(const std::string& source_path,
                   std::string* unique_id,
                   string16* device_label,
                   uint64* storage_size_in_bytes) {
  const disks::DiskMountManager::Disk* disk =
      disks::DiskMountManager::GetInstance()->FindDiskBySourcePath(source_path);
  if (!disk || disk->device_type() == DEVICE_TYPE_UNKNOWN)
    return false;

  if (unique_id)
    *unique_id = MakeDeviceUniqueId(*disk);

  if (device_label)
    *device_label = GetDeviceName(*disk);

  if (storage_size_in_bytes)
    *storage_size_in_bytes = disk->total_size_in_bytes();
  return true;
}

}  // namespace

using content::BrowserThread;

RemovableDeviceNotificationsCros::RemovableDeviceNotificationsCros() {
  DCHECK(disks::DiskMountManager::GetInstance());
  disks::DiskMountManager::GetInstance()->AddObserver(this);
  CheckExistingMountPointsOnUIThread();
}

RemovableDeviceNotificationsCros::~RemovableDeviceNotificationsCros() {
  disks::DiskMountManager* manager = disks::DiskMountManager::GetInstance();
  if (manager) {
    manager->RemoveObserver(this);
  }
}

void RemovableDeviceNotificationsCros::CheckExistingMountPointsOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const disks::DiskMountManager::MountPointMap& mount_point_map =
      disks::DiskMountManager::GetInstance()->mount_points();
  for (disks::DiskMountManager::MountPointMap::const_iterator it =
           mount_point_map.begin(); it != mount_point_map.end(); ++it) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(
            &RemovableDeviceNotificationsCros::CheckMountedPathOnFileThread,
            this, it->second));
  }
}

void RemovableDeviceNotificationsCros::OnDiskEvent(
    disks::DiskMountManager::DiskEvent event,
    const disks::DiskMountManager::Disk* disk) {
}

void RemovableDeviceNotificationsCros::OnDeviceEvent(
    disks::DiskMountManager::DeviceEvent event,
    const std::string& device_path) {
}

void RemovableDeviceNotificationsCros::OnMountEvent(
    disks::DiskMountManager::MountEvent event,
    MountError error_code,
    const disks::DiskMountManager::MountPointInfo& mount_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Ignore mount points that are not devices.
  if (mount_info.mount_type != MOUNT_TYPE_DEVICE)
    return;
  // Ignore errors.
  if (error_code != MOUNT_ERROR_NONE)
    return;
  if (mount_info.mount_condition != disks::MOUNT_CONDITION_NONE)
    return;

  switch (event) {
    case disks::DiskMountManager::MOUNTING: {
      if (ContainsKey(mount_map_, mount_info.mount_path)) {
        NOTREACHED();
        return;
      }

      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(
              &RemovableDeviceNotificationsCros::CheckMountedPathOnFileThread,
              this, mount_info));
      break;
    }
    case disks::DiskMountManager::UNMOUNTING: {
      MountMap::iterator it = mount_map_.find(mount_info.mount_path);
      if (it == mount_map_.end())
        return;
      receiver()->ProcessDetach(it->second.storage_info.device_id);
      mount_map_.erase(it);
      break;
    }
  }
}

void RemovableDeviceNotificationsCros::OnFormatEvent(
    disks::DiskMountManager::FormatEvent event,
    FormatError error_code,
    const std::string& device_path) {
}

bool RemovableDeviceNotificationsCros::GetDeviceInfoForPath(
    const base::FilePath& path,
    StorageInfo* device_info) const {
  if (!path.IsAbsolute())
    return false;

  base::FilePath current = path;
  while (!ContainsKey(mount_map_, current.value()) &&
         current != current.DirName()) {
    current = current.DirName();
  }

  MountMap::const_iterator info_it = mount_map_.find(current.value());
  if (info_it == mount_map_.end())
    return false;

  if (device_info)
    *device_info = info_it->second.storage_info;
  return true;
}

uint64 RemovableDeviceNotificationsCros::GetStorageSize(
    const std::string& device_location) const {
  MountMap::const_iterator info_it = mount_map_.find(device_location);
  return (info_it != mount_map_.end()) ?
      info_it->second.storage_size_in_bytes : 0;
}

void RemovableDeviceNotificationsCros::CheckMountedPathOnFileThread(
    const disks::DiskMountManager::MountPointInfo& mount_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  bool has_dcim = chrome::IsMediaDevice(mount_info.mount_path);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RemovableDeviceNotificationsCros::AddMountedPathOnUIThread,
                 this, mount_info, has_dcim));
}

void RemovableDeviceNotificationsCros::AddMountedPathOnUIThread(
    const disks::DiskMountManager::MountPointInfo& mount_info, bool has_dcim) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (ContainsKey(mount_map_, mount_info.mount_path)) {
    // CheckExistingMountPointsOnUIThread() added the mount point information
    // in the map before the device attached handler is called. Therefore, an
    // entry for the device already exists in the map.
    return;
  }

  // Get the media device uuid and label if exists.
  std::string unique_id;
  string16 device_label;
  uint64 storage_size_in_bytes;
  if (!GetDeviceInfo(mount_info.source_path, &unique_id, &device_label,
                     &storage_size_in_bytes))
    return;

  // Keep track of device uuid and label, to see how often we receive empty
  // values.
  chrome::MediaStorageUtil::RecordDeviceInfoHistogram(true, unique_id,
                                                      device_label);
  if (unique_id.empty() || device_label.empty())
    return;

  chrome::MediaStorageUtil::Type type = has_dcim ?
      chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM :
      chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM;

  std::string device_id = chrome::MediaStorageUtil::MakeDeviceId(type,
                                                                 unique_id);
  StorageObjectInfo object_info = {
      StorageInfo(device_id, device_label, mount_info.mount_path),
      storage_size_in_bytes
  };
  mount_map_.insert(std::make_pair(mount_info.mount_path, object_info));
  receiver()->ProcessAttach(
      device_id,
      chrome::GetDisplayNameForDevice(storage_size_in_bytes, device_label),
      mount_info.mount_path);
}

}  // namespace chromeos
