// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/system_monitor/media_transfer_protocol_device_observer_chromeos.h"

#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/system_monitor/system_monitor.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/mtp/media_transfer_protocol_manager.h"
#include "chrome/browser/system_monitor/removable_device_constants.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "chromeos/dbus/mtp_storage_info.pb.h"

namespace chromeos {
namespace mtp {

using chrome::MediaStorageUtil;

namespace {

// Device root path constant.
const char kRootPath[] = "/";

// Returns the storage identifier of the device from the given |storage_name|.
// E.g. If the |storage_name| is "usb:2,2:65537", the storage identifier is
// "65537".
std::string GetStorageIdFromStorageName(const std::string& storage_name) {
  std::vector<std::string> name_parts;
  base::SplitString(storage_name, ':', &name_parts);
  return name_parts.size() == 3 ? name_parts[2] : std::string();
}

// Returns a unique device id from the given |storage_info|.
std::string GetDeviceIdFromStorageInfo(const MtpStorageInfo& storage_info) {
  const std::string storage_id =
      GetStorageIdFromStorageName(storage_info.storage_name());
  if (storage_id.empty())
    return std::string();

  // Some devices have multiple data stores. Therefore, include storage id as
  // part of unique id along with vendor, model and volume information.
  const std::string vendor_id = base::UintToString(storage_info.vendor_id());
  const std::string model_id = base::UintToString(storage_info.product_id());
  const std::string& volume_id = storage_info.volume_identifier();
  std::string unique_id = base::StringPrintf(
      "%s%s%s%s%s%s%s%s",
      chrome::kVendorModelVolumeStoragePrefix,
      vendor_id.c_str(),
      chrome::kNonSpaceDelim,
      model_id.c_str(),
      chrome::kNonSpaceDelim,
      volume_id.c_str(),
      chrome::kNonSpaceDelim,
      storage_id.c_str());
  return MediaStorageUtil::MakeDeviceId(MediaStorageUtil::MTP_OR_PTP,
                                        unique_id);
}

// Helper function to get device label from storage information.
string16 GetDeviceLabelFromStorageInfo(const MtpStorageInfo& storage_info) {
  std::string device_label;
  const std::string& vendor_name = storage_info.vendor();
  device_label = vendor_name;

  const std::string& product_name = storage_info.product();
  if (!product_name.empty()) {
    if (!device_label.empty())
      device_label += chrome::kSpaceDelim;
    device_label += product_name;
  }
  return UTF8ToUTF16(device_label);
}

// Helper function to get the device storage details such as device id, label
// and location. On success and fills in |id|, |label| and |location|.
void GetStorageInfo(const std::string& storage_name,
                    std::string* id,
                    string16* label,
                    std::string* location) {
  DCHECK(!storage_name.empty());
  MediaTransferProtocolManager* mtp_manager =
      MediaTransferProtocolManager::GetInstance();
  DCHECK(mtp_manager);
  const MtpStorageInfo* storage_info =
      mtp_manager->GetStorageInfo(storage_name);

  if (!storage_info)
    return;

  if (id)
    *id = GetDeviceIdFromStorageInfo(*storage_info);

  if (label)
    *label = GetDeviceLabelFromStorageInfo(*storage_info);

  // Construct a dummy device path using the storage name. This is only used
  // for registering device media file system.
  // E.g.: /usb:2,2:12345
  if (location)
    *location = kRootPath + storage_name;
}

}  // namespace

MediaTransferProtocolDeviceObserverCros::
MediaTransferProtocolDeviceObserverCros()
    : get_storage_info_func_(&GetStorageInfo) {
  MediaTransferProtocolManager* mtp_manager =
      MediaTransferProtocolManager::GetInstance();
  if (mtp_manager)
    mtp_manager->AddObserver(this);
  EnumerateStorages();
}

// This constructor is only used by unit tests.
MediaTransferProtocolDeviceObserverCros::
    MediaTransferProtocolDeviceObserverCros(
        GetStorageInfoFunc get_storage_info_func)
    : get_storage_info_func_(get_storage_info_func) {
  // In unit tests, we don't have a media transfer protocol manager.
  DCHECK(!MediaTransferProtocolManager::GetInstance());
}

MediaTransferProtocolDeviceObserverCros::
~MediaTransferProtocolDeviceObserverCros() {
  MediaTransferProtocolManager* mtp_manager =
      MediaTransferProtocolManager::GetInstance();
  if (mtp_manager)
    mtp_manager->RemoveObserver(this);
}

// MediaTransferProtocolManager::Observer override.
void MediaTransferProtocolDeviceObserverCros::StorageChanged(
    bool is_attached,
    const std::string& storage_name) {
  DCHECK(!storage_name.empty());

  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  DCHECK(system_monitor);

  // New storage is attached.
  if (is_attached) {
    std::string device_id;
    string16 device_name;
    std::string location;
    get_storage_info_func_(storage_name, &device_id, &device_name, &location);

    // Keep track of device id and device name to see how often we receive
    // empty values.
    UMA_HISTOGRAM_BOOLEAN("MediaDeviceNotification.MTPDeviceUUIDAvailable",
                          !device_id.empty());
    UMA_HISTOGRAM_BOOLEAN("MediaDeviceNotification.MTPDeviceNameAvailable",
                          !device_name.empty());

    if (device_id.empty() || device_name.empty())
      return;

    DCHECK(!ContainsKey(storage_map_, storage_name));
    storage_map_[storage_name] = device_id;
    system_monitor->ProcessRemovableStorageAttached(device_id, device_name,
                                                    location);
  } else {
    // Existing storage is detached.
    StorageNameToIdMap::iterator it = storage_map_.find(storage_name);
    if (it == storage_map_.end())
      return;
    system_monitor->ProcessRemovableStorageDetached(it->second);
    storage_map_.erase(it);
  }
}

void MediaTransferProtocolDeviceObserverCros::EnumerateStorages() {
  typedef std::vector<std::string> StorageList;
  MediaTransferProtocolManager* mtp_manager =
      MediaTransferProtocolManager::GetInstance();
  DCHECK(mtp_manager);
  StorageList storages = mtp_manager->GetStorages();
  for (StorageList::const_iterator storage_iter = storages.begin();
       storage_iter != storages.end(); ++storage_iter) {
    StorageChanged(true, *storage_iter);
  }
}

}  // namespace mtp
}  // namespace chromeos
