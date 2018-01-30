// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/media_transfer_protocol_device_observer_chromeos.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_info_utils.h"

namespace storage_monitor {

MediaTransferProtocolDeviceObserverChromeOS::
    MediaTransferProtocolDeviceObserverChromeOS(
        StorageMonitor::Receiver* receiver,
        device::MediaTransferProtocolManager* mtp_manager)
    : mtp_manager_(mtp_manager),
      get_mtp_storage_info_cb_(base::BindRepeating(
            &device::MediaTransferProtocolManager::GetStorageInfo,
            base::Unretained(mtp_manager_))),
      notifications_(receiver),
      weak_ptr_factory_(this) {
  mtp_manager_->AddObserver(this);
  EnumerateStorages();
}

// This constructor is only used by unit tests.
MediaTransferProtocolDeviceObserverChromeOS::
    MediaTransferProtocolDeviceObserverChromeOS(
        StorageMonitor::Receiver* receiver,
        device::MediaTransferProtocolManager* mtp_manager,
        const GetMtpStorageInfoCallback& get_mtp_storage_info_cb)
    : mtp_manager_(mtp_manager),
      get_mtp_storage_info_cb_(get_mtp_storage_info_cb),
      notifications_(receiver),
      weak_ptr_factory_(this) {}

MediaTransferProtocolDeviceObserverChromeOS::
    ~MediaTransferProtocolDeviceObserverChromeOS() {
  mtp_manager_->RemoveObserver(this);
}

bool MediaTransferProtocolDeviceObserverChromeOS::GetStorageInfoForPath(
    const base::FilePath& path,
    StorageInfo* storage_info) const {
  DCHECK(storage_info);

  if (!path.IsAbsolute())
    return false;

  std::vector<base::FilePath::StringType> path_components;
  path.GetComponents(&path_components);
  if (path_components.size() < 2)
    return false;

  // First and second component of the path specifies the device location.
  // E.g.: If |path| is "/usb:2,2:65537/DCIM/Folder_a", "/usb:2,2:65537" is the
  // device location.
  StorageLocationToInfoMap::const_iterator info_it =
      storage_map_.find(GetDeviceLocationFromStorageName(path_components[1]));
  if (info_it == storage_map_.end())
    return false;

  *storage_info = info_it->second;
  return true;
}

void MediaTransferProtocolDeviceObserverChromeOS::EjectDevice(
    const std::string& device_id,
    base::Callback<void(StorageMonitor::EjectStatus)> callback) {
  std::string location;
  if (!GetLocationForDeviceId(device_id, &location)) {
    callback.Run(StorageMonitor::EJECT_NO_SUCH_DEVICE);
    return;
  }

  // TODO(thestig): Change this to tell the mtp manager to eject the device.

  StorageChanged(false, location);
  callback.Run(StorageMonitor::EJECT_OK);
}

// device::MediaTransferProtocolManager::Observer override.
void MediaTransferProtocolDeviceObserverChromeOS::StorageChanged(
    bool is_attached,
    const std::string& storage_name) {
  DCHECK(!storage_name.empty());

  // Existing storage is detached.
  if (!is_attached) {
    StorageLocationToInfoMap::iterator it =
        storage_map_.find(GetDeviceLocationFromStorageName(storage_name));
    if (it == storage_map_.end())
      return;
    notifications_->ProcessDetach(it->second.device_id());
    storage_map_.erase(it);
    return;
  }

  // New storage is attached.
  get_mtp_storage_info_cb_.Run(
      storage_name,
      base::BindOnce(
          &MediaTransferProtocolDeviceObserverChromeOS::DoAttachStorage,
          weak_ptr_factory_.GetWeakPtr()));
}

void MediaTransferProtocolDeviceObserverChromeOS::DoAttachStorage(
    const device::mojom::MtpStorageInfo* mtp_storage_info) {
  if (!mtp_storage_info)
    return;

  std::string device_id = GetDeviceIdFromStorageInfo(*mtp_storage_info);
  base::string16 storage_label =
    GetDeviceLabelFromStorageInfo(*mtp_storage_info);
  std::string location =
      GetDeviceLocationFromStorageName(mtp_storage_info->storage_name);
  base::string16 vendor_name = base::UTF8ToUTF16(mtp_storage_info->vendor);
  base::string16 product_name = base::UTF8ToUTF16(mtp_storage_info->product);

  if (device_id.empty() || storage_label.empty())
    return;

  DCHECK(!base::ContainsKey(storage_map_, location));

  StorageInfo storage_info(device_id, location, storage_label, vendor_name,
      product_name, 0);
  storage_map_[location] = storage_info;
  notifications_->ProcessAttach(storage_info);
}

void MediaTransferProtocolDeviceObserverChromeOS::EnumerateStorages() {
  mtp_manager_->GetStorages(base::BindOnce(
        &MediaTransferProtocolDeviceObserverChromeOS::OnReceivedStorages,
        weak_ptr_factory_.GetWeakPtr()));
}

void MediaTransferProtocolDeviceObserverChromeOS::OnReceivedStorages(
    const std::vector<std::string>& storages) {
  typedef std::vector<std::string> StorageList;
  for (StorageList::const_iterator storage_iter = storages.begin();
       storage_iter != storages.end(); ++storage_iter) {
    StorageChanged(true, *storage_iter);
  }
}

bool MediaTransferProtocolDeviceObserverChromeOS::GetLocationForDeviceId(
    const std::string& device_id,
    std::string* location) const {
  for (StorageLocationToInfoMap::const_iterator it = storage_map_.begin();
       it != storage_map_.end(); ++it) {
    if (it->second.device_id() == device_id) {
      *location = it->first;
      return true;
    }
  }

  return false;
}

}  // namespace storage_monitor
