// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StorageMonitorLinux implementation.

#include "chrome/browser/storage_monitor/storage_monitor_linux.h"

#include <mntent.h>
#include <stdio.h>

#include <list>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/storage_monitor/media_transfer_protocol_device_observer_linux.h"
#include "chrome/browser/storage_monitor/removable_device_constants.h"
#include "chrome/browser/storage_monitor/udev_util_linux.h"
#include "chrome/common/chrome_switches.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"

namespace chrome {

using content::BrowserThread;
typedef MtabWatcherLinux::MountPointDeviceMap MountPointDeviceMap;

namespace {

// udev device property constants.
const char kBlockSubsystemKey[] = "block";
const char kDiskDeviceTypeKey[] = "disk";
const char kFsUUID[] = "ID_FS_UUID";
const char kLabel[] = "ID_FS_LABEL";
const char kModel[] = "ID_MODEL";
const char kModelID[] = "ID_MODEL_ID";
const char kRemovableSysAttr[] = "removable";
const char kSerialShort[] = "ID_SERIAL_SHORT";
const char kSizeSysAttr[] = "size";
const char kVendor[] = "ID_VENDOR";
const char kVendorID[] = "ID_VENDOR_ID";

// Construct a device id using label or manufacturer (vendor and model) details.
std::string MakeDeviceUniqueId(struct udev_device* device) {
  std::string uuid = GetUdevDevicePropertyValue(device, kFsUUID);
  if (!uuid.empty())
    return kFSUniqueIdPrefix + uuid;

  // If one of the vendor, model, serial information is missing, its value
  // in the string is empty.
  // Format: VendorModelSerial:VendorInfo:ModelInfo:SerialShortInfo
  // E.g.: VendorModelSerial:Kn:DataTravel_12.10:8000000000006CB02CDB
  std::string vendor = GetUdevDevicePropertyValue(device, kVendorID);
  std::string model = GetUdevDevicePropertyValue(device, kModelID);
  std::string serial_short = GetUdevDevicePropertyValue(device,
                                                        kSerialShort);
  if (vendor.empty() && model.empty() && serial_short.empty())
    return std::string();

  return kVendorModelSerialPrefix + vendor + ":" + model + ":" + serial_short;
}

// Records GetDeviceInfo result on destruction, to see how often we fail to get
// device details.
class ScopedGetDeviceInfoResultRecorder {
 public:
  ScopedGetDeviceInfoResultRecorder() : result_(false) {}
  ~ScopedGetDeviceInfoResultRecorder() {
    UMA_HISTOGRAM_BOOLEAN("MediaDeviceNotification.UdevRequestSuccess",
                          result_);
  }

  void set_result(bool result) {
    result_ = result;
  }

 private:
  bool result_;

  DISALLOW_COPY_AND_ASSIGN(ScopedGetDeviceInfoResultRecorder);
};

// Returns the storage partition size of the device specified by |device_path|.
// If the requested information is unavailable, returns 0.
uint64 GetDeviceStorageSize(const base::FilePath& device_path,
                            struct udev_device* device) {
  // sysfs provides the device size in units of 512-byte blocks.
  const std::string partition_size = udev_device_get_sysattr_value(
      device, kSizeSysAttr);

  // Keep track of device size, to see how often this information is
  // unavailable.
  UMA_HISTOGRAM_BOOLEAN(
      "RemovableDeviceNotificationsLinux.device_partition_size_available",
      !partition_size.empty());

  uint64 total_size_in_bytes = 0;
  if (!base::StringToUint64(partition_size, &total_size_in_bytes))
    return 0;
  return (total_size_in_bytes <= kuint64max / 512) ?
      total_size_in_bytes * 512 : 0;
}

// Constructs the device name from the device properties. If the device details
// are unavailable, returns an empty string.
string16 GetDeviceName(struct udev_device* device,
                       string16* out_volume_label,
                       string16* out_vendor_name,
                       string16* out_model_name) {
  std::string device_label = GetUdevDevicePropertyValue(device, kLabel);
  std::string vendor_name = GetUdevDevicePropertyValue(device, kVendor);
  std::string model_name = GetUdevDevicePropertyValue(device, kModel);
  if (out_volume_label)
    *out_volume_label = UTF8ToUTF16(device_label);
  if (out_vendor_name)
    *out_vendor_name = UTF8ToUTF16(vendor_name);
  if (out_model_name)
    *out_model_name = UTF8ToUTF16(model_name);

  if (!device_label.empty() && IsStringUTF8(device_label))
    return UTF8ToUTF16(device_label);

  device_label = GetUdevDevicePropertyValue(device, kFsUUID);
  // Keep track of device uuid, to see how often we receive empty uuid values.
  UMA_HISTOGRAM_BOOLEAN(
      "RemovableDeviceNotificationsLinux.device_file_system_uuid_available",
      !device_label.empty());

  const string16 name = MediaStorageUtil::GetFullProductName(vendor_name,
                                                             model_name);

  const string16 device_label_utf16 =
      (!device_label.empty() && IsStringUTF8(device_label)) ?
          UTF8ToUTF16(device_label) : string16();
  if (!name.empty() && !device_label_utf16.empty())
    return device_label_utf16 + ASCIIToUTF16(" ") + name;
  return name.empty() ? device_label_utf16 : name;
}

// Gets the device information using udev library.
scoped_ptr<StorageInfo> GetDeviceInfo(const base::FilePath& device_path,
                                      const base::FilePath& mount_point) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!device_path.empty());

  scoped_ptr<StorageInfo> storage_info;

  ScopedGetDeviceInfoResultRecorder results_recorder;

  ScopedUdevObject udev_obj(udev_new());
  if (!udev_obj.get())
    return storage_info.Pass();

  struct stat device_stat;
  if (stat(device_path.value().c_str(), &device_stat) < 0)
    return storage_info.Pass();

  char device_type;
  if (S_ISCHR(device_stat.st_mode))
    device_type = 'c';
  else if (S_ISBLK(device_stat.st_mode))
    device_type = 'b';
  else
    return storage_info.Pass();  // Not a supported type.

  ScopedUdevDeviceObject device(
      udev_device_new_from_devnum(udev_obj, device_type, device_stat.st_rdev));
  if (!device.get())
    return storage_info.Pass();

  string16 volume_label;
  string16 vendor_name;
  string16 model_name;
  string16 device_name = GetDeviceName(device, &volume_label,
                                       &vendor_name, &model_name);
  std::string unique_id = MakeDeviceUniqueId(device);

  // Keep track of device info details to see how often we get invalid values.
  MediaStorageUtil::RecordDeviceInfoHistogram(true, unique_id, device_name);

  const char* value = udev_device_get_sysattr_value(device, kRemovableSysAttr);
  if (!value) {
    // |parent_device| is owned by |device| and does not need to be cleaned
    // up.
    struct udev_device* parent_device =
        udev_device_get_parent_with_subsystem_devtype(device,
                                                      kBlockSubsystemKey,
                                                      kDiskDeviceTypeKey);
    value = udev_device_get_sysattr_value(parent_device, kRemovableSysAttr);
  }
  const bool is_removable = (value && atoi(value) == 1);

  MediaStorageUtil::Type type = MediaStorageUtil::FIXED_MASS_STORAGE;
  if (is_removable) {
    if (MediaStorageUtil::HasDcim(mount_point.value()))
      type = MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM;
    else
      type = MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM;
  }

  results_recorder.set_result(true);

  storage_info.reset(new StorageInfo(
      MediaStorageUtil::MakeDeviceId(type, unique_id),
      device_name,
      mount_point.value(),
      volume_label,
      vendor_name,
      model_name,
      GetDeviceStorageSize(device_path, device)));
  return storage_info.Pass();
}

MtabWatcherLinux* CreateMtabWatcherLinuxOnFileThread(
    const base::FilePath& mtab_path,
    base::WeakPtr<MtabWatcherLinux::Delegate> delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // Owned by caller.
  return new MtabWatcherLinux(mtab_path, delegate);
}

}  // namespace

StorageMonitorLinux::StorageMonitorLinux(const base::FilePath& path)
    : mtab_path_(path),
      get_device_info_callback_(base::Bind(&GetDeviceInfo)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

StorageMonitorLinux::~StorageMonitorLinux() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType))
    device::MediaTransferProtocolManager::Shutdown();
}

void StorageMonitorLinux::Init() {
  DCHECK(!mtab_path_.empty());

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&CreateMtabWatcherLinuxOnFileThread,
                 mtab_path_,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&StorageMonitorLinux::OnMtabWatcherCreated,
                 weak_ptr_factory_.GetWeakPtr()));

  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    scoped_refptr<base::MessageLoopProxy> loop_proxy;
    loop_proxy = content::BrowserThread::GetMessageLoopProxyForThread(
        content::BrowserThread::FILE);
    device::MediaTransferProtocolManager::Initialize(loop_proxy);

    media_transfer_protocol_device_observer_.reset(
        new MediaTransferProtocolDeviceObserverLinux());
    media_transfer_protocol_device_observer_->SetNotifications(receiver());
  }
}

bool StorageMonitorLinux::GetStorageInfoForPath(
    const base::FilePath& path,
    StorageInfo* device_info) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!path.IsAbsolute())
    return false;

  base::FilePath current = path;
  while (!ContainsKey(mount_info_map_, current) && current != current.DirName())
    current = current.DirName();

  MountMap::const_iterator mount_info = mount_info_map_.find(current);
  if (mount_info == mount_info_map_.end())
    return false;
  if (device_info)
    *device_info = mount_info->second.storage_info;
  return true;
}

uint64 StorageMonitorLinux::GetStorageSize(const std::string& location) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  MountMap::const_iterator mount_info = mount_info_map_.find(
      base::FilePath(location));
  return (mount_info != mount_info_map_.end()) ?
      mount_info->second.storage_info.total_size_in_bytes : 0;
}

void StorageMonitorLinux::SetGetDeviceInfoCallbackForTest(
    const GetDeviceInfoCallback& get_device_info_callback) {
  get_device_info_callback_ = get_device_info_callback;
}

void StorageMonitorLinux::OnMtabWatcherCreated(MtabWatcherLinux* watcher) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  mtab_watcher_.reset(watcher);
}

void StorageMonitorLinux::UpdateMtab(const MountPointDeviceMap& new_mtab) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Check existing mtab entries for unaccounted mount points.
  // These mount points must have been removed in the new mtab.
  std::list<base::FilePath> mount_points_to_erase;
  std::list<base::FilePath> multiple_mounted_devices_needing_reattachment;
  for (MountMap::const_iterator old_iter = mount_info_map_.begin();
       old_iter != mount_info_map_.end(); ++old_iter) {
    const base::FilePath& mount_point = old_iter->first;
    const base::FilePath& mount_device = old_iter->second.mount_device;
    MountPointDeviceMap::const_iterator new_iter = new_mtab.find(mount_point);
    // |mount_point| not in |new_mtab| or |mount_device| is no longer mounted at
    // |mount_point|.
    if (new_iter == new_mtab.end() || (new_iter->second != mount_device)) {
      MountPriorityMap::iterator priority =
          mount_priority_map_.find(mount_device);
      DCHECK(priority != mount_priority_map_.end());
      ReferencedMountPoint::const_iterator has_priority =
          priority->second.find(mount_point);
      if (MediaStorageUtil::IsRemovableDevice(
              old_iter->second.storage_info.device_id)) {
        DCHECK(has_priority != priority->second.end());
        if (has_priority->second) {
          receiver()->ProcessDetach(old_iter->second.storage_info.device_id);
        }
        if (priority->second.size() > 1)
          multiple_mounted_devices_needing_reattachment.push_back(mount_device);
      }
      priority->second.erase(mount_point);
      if (priority->second.empty())
        mount_priority_map_.erase(mount_device);
      mount_points_to_erase.push_back(mount_point);
    }
  }

  // Erase the |mount_info_map_| entries afterwards. Erasing in the loop above
  // using the iterator is slightly more efficient, but more tricky, since
  // calling std::map::erase() on an iterator invalidates it.
  for (std::list<base::FilePath>::const_iterator it =
           mount_points_to_erase.begin();
       it != mount_points_to_erase.end();
       ++it) {
    mount_info_map_.erase(*it);
  }

  // For any multiply mounted device where the mount that we had notified
  // got detached, send a notification of attachment for one of the other
  // mount points.
  for (std::list<base::FilePath>::const_iterator it =
           multiple_mounted_devices_needing_reattachment.begin();
       it != multiple_mounted_devices_needing_reattachment.end();
       ++it) {
    ReferencedMountPoint::iterator first_mount_point_info =
        mount_priority_map_.find(*it)->second.begin();
    const base::FilePath& mount_point = first_mount_point_info->first;
    first_mount_point_info->second = true;

    const StorageInfo& mount_info =
        mount_info_map_.find(mount_point)->second.storage_info;
    DCHECK(MediaStorageUtil::IsRemovableDevice(mount_info.device_id));
    receiver()->ProcessAttach(mount_info);
  }

  // Check new mtab entries against existing ones.
  for (MountPointDeviceMap::const_iterator new_iter = new_mtab.begin();
       new_iter != new_mtab.end(); ++new_iter) {
    const base::FilePath& mount_point = new_iter->first;
    const base::FilePath& mount_device = new_iter->second;
    MountMap::iterator old_iter = mount_info_map_.find(mount_point);
    if (old_iter == mount_info_map_.end() ||
        old_iter->second.mount_device != mount_device) {
      // New mount point found or an existing mount point found with a new
      // device.
      if (IsDeviceAlreadyMounted(mount_device)) {
        HandleDeviceMountedMultipleTimes(mount_device, mount_point);
      } else {
        BrowserThread::PostTaskAndReplyWithResult(
            BrowserThread::FILE, FROM_HERE,
            base::Bind(get_device_info_callback_, mount_device, mount_point),
            base::Bind(&StorageMonitorLinux::AddNewMount,
                       weak_ptr_factory_.GetWeakPtr(),
                       mount_device));
      }
    }
  }
}

bool StorageMonitorLinux::IsDeviceAlreadyMounted(
    const base::FilePath& mount_device) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return ContainsKey(mount_priority_map_, mount_device);
}

void StorageMonitorLinux::HandleDeviceMountedMultipleTimes(
    const base::FilePath& mount_device,
    const base::FilePath& mount_point) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  MountPriorityMap::iterator priority = mount_priority_map_.find(mount_device);
  DCHECK(priority != mount_priority_map_.end());
  const base::FilePath& other_mount_point = priority->second.begin()->first;
  priority->second[mount_point] = false;
  mount_info_map_[mount_point] =
      mount_info_map_.find(other_mount_point)->second;
}

void StorageMonitorLinux::AddNewMount(const base::FilePath& mount_device,
                                      scoped_ptr<StorageInfo> storage_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!storage_info)
    return;

  DCHECK(!storage_info->device_id.empty());
  DCHECK(!storage_info->name.empty());

  bool removable = MediaStorageUtil::IsRemovableDevice(storage_info->device_id);
  const base::FilePath mount_point(storage_info->location);

  MountPointInfo mount_point_info;
  mount_point_info.mount_device = mount_device;
  mount_point_info.storage_info = *storage_info;
  mount_info_map_[mount_point] = mount_point_info;
  mount_priority_map_[mount_device][mount_point] = removable;

  if (removable) {
    // TODO(gbillock) Do this in a higher level instead of here.
    storage_info->name = MediaStorageUtil::GetDisplayNameForDevice(
        storage_info->total_size_in_bytes, storage_info->name);
    receiver()->ProcessAttach(*storage_info);
  }
}

}  // namespace chrome
