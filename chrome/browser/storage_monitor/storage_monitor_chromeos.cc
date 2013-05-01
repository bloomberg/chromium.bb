// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chromeos::StorageMonitorCros implementation.

#include "chrome/browser/storage_monitor/storage_monitor_chromeos.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/storage_monitor/media_transfer_protocol_device_observer_linux.h"
#include "chrome/browser/storage_monitor/removable_device_constants.h"
#include "chrome/browser/storage_monitor/test_media_transfer_protocol_manager_linux.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"

namespace chromeos {

namespace {

// Constructs a device name using label or manufacturer (vendor and product)
// name details.
string16 GetDeviceName(const disks::DiskMountManager::Disk& disk,
                       string16* storage_label,
                       string16* vendor_name,
                       string16* model_name) {
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

  if (storage_label)
    *storage_label = UTF8ToUTF16(device_label);
  if (vendor_name)
    *vendor_name = UTF8ToUTF16(disk.vendor_name());
  if (model_name)
    *model_name = UTF8ToUTF16(disk.product_name());

  if (!device_label.empty() && IsStringUTF8(device_label))
    return UTF8ToUTF16(device_label);

  return chrome::MediaStorageUtil::GetFullProductName(disk.vendor_name(),
                                                      disk.product_name());
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
                   uint64* storage_size_in_bytes,
                   string16* storage_label,
                   string16* vendor_name,
                   string16* model_name) {
  const disks::DiskMountManager::Disk* disk =
      disks::DiskMountManager::GetInstance()->FindDiskBySourcePath(source_path);
  if (!disk || disk->device_type() == DEVICE_TYPE_UNKNOWN)
    return false;

  if (unique_id)
    *unique_id = MakeDeviceUniqueId(*disk);

  if (device_label)
    *device_label = GetDeviceName(*disk, storage_label,
                                  vendor_name, model_name);

  if (storage_size_in_bytes)
    *storage_size_in_bytes = disk->total_size_in_bytes();
  return true;
}

// Returns whether the mount point in |mount_info| is a media device or not.
bool CheckMountedPathOnFileThread(
    const disks::DiskMountManager::MountPointInfo& mount_info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  return chrome::MediaStorageUtil::HasDcim(
      base::FilePath(mount_info.mount_path));
}

}  // namespace

using content::BrowserThread;
using chrome::StorageInfo;

StorageMonitorCros::StorageMonitorCros()
    : weak_ptr_factory_(this) {
  // TODO(thestig) Do not do this here. Do it in TestingBrowserProcess when
  // BrowserProcess owns StorageMonitor.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    SetMediaTransferProtocolManagerForTest(
        new chrome::TestMediaTransferProtocolManagerLinux());
  }
}

StorageMonitorCros::~StorageMonitorCros() {
  disks::DiskMountManager* manager = disks::DiskMountManager::GetInstance();
  if (manager) {
    manager->RemoveObserver(this);
  }
}

void StorageMonitorCros::Init() {
  DCHECK(disks::DiskMountManager::GetInstance());
  disks::DiskMountManager::GetInstance()->AddObserver(this);
  CheckExistingMountPoints();

  if (!media_transfer_protocol_manager_) {
    scoped_refptr<base::MessageLoopProxy> loop_proxy;
    media_transfer_protocol_manager_.reset(
        device::MediaTransferProtocolManager::Initialize(loop_proxy));
  }

  media_transfer_protocol_device_observer_.reset(
      new chrome::MediaTransferProtocolDeviceObserverLinux(receiver()));
}

void StorageMonitorCros::CheckExistingMountPoints() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const disks::DiskMountManager::MountPointMap& mount_point_map =
      disks::DiskMountManager::GetInstance()->mount_points();
  for (disks::DiskMountManager::MountPointMap::const_iterator it =
           mount_point_map.begin(); it != mount_point_map.end(); ++it) {
    BrowserThread::PostTaskAndReplyWithResult(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&CheckMountedPathOnFileThread, it->second),
        base::Bind(&StorageMonitorCros::AddMountedPath,
                   weak_ptr_factory_.GetWeakPtr(), it->second));
  }

  // Note: relies on scheduled tasks on the file thread being sequential. This
  // block needs to follow the for loop, so that the DoNothing call on the FILE
  // thread happens after the scheduled metadata retrievals, meaning that the
  // reply callback will then happen after all the AddNewMount calls.
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&base::DoNothing),
      base::Bind(&StorageMonitorCros::MarkInitialized,
                 weak_ptr_factory_.GetWeakPtr()));
}

void StorageMonitorCros::OnDiskEvent(
    disks::DiskMountManager::DiskEvent event,
    const disks::DiskMountManager::Disk* disk) {
}

void StorageMonitorCros::OnDeviceEvent(
    disks::DiskMountManager::DeviceEvent event,
    const std::string& device_path) {
}

void StorageMonitorCros::OnMountEvent(
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

      BrowserThread::PostTaskAndReplyWithResult(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&CheckMountedPathOnFileThread, mount_info),
          base::Bind(&StorageMonitorCros::AddMountedPath,
                     weak_ptr_factory_.GetWeakPtr(), mount_info));
      break;
    }
    case disks::DiskMountManager::UNMOUNTING: {
      MountMap::iterator it = mount_map_.find(mount_info.mount_path);
      if (it == mount_map_.end())
        return;
      receiver()->ProcessDetach(it->second.device_id);
      mount_map_.erase(it);
      break;
    }
  }
}

void StorageMonitorCros::OnFormatEvent(
    disks::DiskMountManager::FormatEvent event,
    FormatError error_code,
    const std::string& device_path) {
}

void StorageMonitorCros::SetMediaTransferProtocolManagerForTest(
    device::MediaTransferProtocolManager* test_manager) {
  DCHECK(!media_transfer_protocol_manager_);
  media_transfer_protocol_manager_.reset(test_manager);
}


bool StorageMonitorCros::GetStorageInfoForPath(
    const base::FilePath& path,
    StorageInfo* device_info) const {
  if (media_transfer_protocol_device_observer_->GetStorageInfoForPath(
          path, device_info)) {
    return true;
  }

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
    *device_info = info_it->second;
  return true;
}

// Callback executed when the unmount call is run by DiskMountManager.
// Forwards result to |EjectDevice| caller.
void NotifyUnmountResult(
    base::Callback<void(chrome::StorageMonitor::EjectStatus)> callback,
    chromeos::MountError error_code) {
  if (error_code == MOUNT_ERROR_NONE)
    callback.Run(chrome::StorageMonitor::EJECT_OK);
  else
    callback.Run(chrome::StorageMonitor::EJECT_FAILURE);
}

void StorageMonitorCros::EjectDevice(
    const std::string& device_id,
    base::Callback<void(EjectStatus)> callback) {
  std::string mount_path;
  for (MountMap::const_iterator info_it = mount_map_.begin();
       info_it != mount_map_.end(); ++info_it) {
    if (info_it->second.device_id == device_id)
      mount_path = info_it->first;
  }

  if (mount_path.empty()) {
    callback.Run(EJECT_NO_SUCH_DEVICE);
    return;
  }

  disks::DiskMountManager* manager = disks::DiskMountManager::GetInstance();
  if (!manager) {
    callback.Run(EJECT_FAILURE);
    return;
  }

  manager->UnmountPath(mount_path, chromeos::UNMOUNT_OPTIONS_NONE,
                       base::Bind(NotifyUnmountResult, callback));
}

device::MediaTransferProtocolManager*
StorageMonitorCros::media_transfer_protocol_manager() {
  return media_transfer_protocol_manager_.get();
}

void StorageMonitorCros::AddMountedPath(
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
  string16 storage_label;
  string16 vendor_name;
  string16 model_name;
  uint64 storage_size_in_bytes;
  if (!GetDeviceInfo(mount_info.source_path, &unique_id, &device_label,
                     &storage_size_in_bytes, &storage_label,
                     &vendor_name, &model_name))
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

  chrome::StorageInfo object_info(
      device_id,
      chrome::MediaStorageUtil::GetDisplayNameForDevice(storage_size_in_bytes,
                                                        device_label),
      mount_info.mount_path,
      storage_label,
      vendor_name,
      model_name,
      storage_size_in_bytes);

  mount_map_.insert(std::make_pair(mount_info.mount_path, object_info));

  receiver()->ProcessAttach(object_info);
}

}  // namespace chromeos
