// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// RemovableDeviceNotificationsLinux implementation.

#include "chrome/browser/system_monitor/removable_device_notifications_linux.h"

#include <libudev.h>
#include <mntent.h>
#include <stdio.h>

#include <list>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/memory/scoped_generic_obj.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/system_monitor/system_monitor.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/system_monitor/media_device_notifications_utils.h"
#include "chrome/browser/system_monitor/removable_device_constants.h"
#include "chrome/browser/system_monitor/media_storage_util.h"

namespace chrome {

using base::SystemMonitor;
using content::BrowserThread;

namespace {

static RemovableDeviceNotificationsLinux*
    g_removable_device_notifications_linux = NULL;

// List of file systems we care about.
const char* const kKnownFileSystems[] = {
  "ext2",
  "ext3",
  "ext4",
  "fat",
  "hfsplus",
  "iso9660",
  "msdos",
  "ntfs",
  "udf",
  "vfat",
};

// udev device property constants.
const char kBlockSubsystemKey[] = "block";
const char kDevName[] = "DEVNAME";
const char kDiskDeviceTypeKey[] = "disk";
const char kFsUUID[] = "ID_FS_UUID";
const char kLabel[] = "ID_FS_LABEL";
const char kModel[] = "ID_MODEL";
const char kModelID[] = "ID_MODEL_ID";
const char kRemovableSysAttr[] = "removable";
const char kSerial[] = "ID_SERIAL";
const char kSerialShort[] = "ID_SERIAL_SHORT";
const char kVendor[] = "ID_VENDOR";
const char kVendorID[] = "ID_VENDOR_ID";

// (mount point, mount device)
// A mapping from mount point to mount device, as extracted from the mtab
// file.
typedef std::map<FilePath, FilePath> MountPointDeviceMap;

// Reads mtab file entries into |mtab|.
void ReadMtab(const FilePath& mtab_path,
              const std::set<std::string>& interesting_file_systems,
              MountPointDeviceMap* mtab) {
  mtab->clear();

  FILE* fp = setmntent(mtab_path.value().c_str(), "r");
  if (!fp)
    return;

  mntent entry;
  char buf[512];

  // We return the same device mounted to multiple locations, but hide
  // devices that have been mounted over.
  while (getmntent_r(fp, &entry, buf, sizeof(buf))) {
    // We only care about real file systems.
    if (!ContainsKey(interesting_file_systems, entry.mnt_type))
      continue;

    (*mtab)[FilePath(entry.mnt_dir)] = FilePath(entry.mnt_fsname);
  }
  endmntent(fp);
}

// ScopedGenericObj functor for UdevObjectRelease().
class ScopedReleaseUdevObject {
 public:
  void operator()(struct udev* udev) const {
    udev_unref(udev);
  }
};
typedef ScopedGenericObj<struct udev*,
                         ScopedReleaseUdevObject> ScopedUdevObject;

// ScopedGenericObj functor for UdevDeviceObjectRelease().
class ScopedReleaseUdevDeviceObject {
 public:
  void operator()(struct udev_device* device) const {
    udev_device_unref(device);
  }
};
typedef ScopedGenericObj<struct udev_device*,
                         ScopedReleaseUdevDeviceObject> ScopedUdevDeviceObject;

// Wrapper function for udev_device_get_property_value() that also checks for
// valid but empty values.
std::string GetUdevDevicePropertyValue(struct udev_device* udev_device,
                                       const char* key) {
  const char* value = udev_device_get_property_value(udev_device, key);
  if (!value)
    return std::string();
  return (strlen(value) > 0) ? value : std::string();
}

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

  return base::StringPrintf("%s%s%s%s%s%s",
                            kVendorModelSerialPrefix,
                            vendor.c_str(), kNonSpaceDelim,
                            model.c_str(), kNonSpaceDelim,
                            serial_short.c_str());
}

// Records GetDeviceInfo result, to see how often we fail to get device details.
void RecordGetDeviceInfoResult(bool result) {
  UMA_HISTOGRAM_BOOLEAN("MediaDeviceNotification.UdevRequestSuccess", result);
}

// Get the device information using udev library.
// On success, returns true and fill in |unique_id|, |name|, and |removable|.
void GetDeviceInfo(const FilePath& device_path, std::string* unique_id,
                   string16* name, bool* removable) {
  DCHECK(!device_path.empty());

  ScopedUdevObject udev_obj(udev_new());
  if (!udev_obj.get()) {
    RecordGetDeviceInfoResult(false);
    return;
  }

  struct stat device_stat;
  if (stat(device_path.value().c_str(), &device_stat) < 0) {
    RecordGetDeviceInfoResult(false);
    return;
  }

  char device_type;
  if (S_ISCHR(device_stat.st_mode)) {
    device_type = 'c';
  } else if (S_ISBLK(device_stat.st_mode)) {
    device_type = 'b';
  } else {
    RecordGetDeviceInfoResult(false);
    return;  // Not a supported type.
  }

  ScopedUdevDeviceObject device(
      udev_device_new_from_devnum(udev_obj, device_type, device_stat.st_rdev));
  if (!device.get()) {
    RecordGetDeviceInfoResult(false);
    return;
  }

  // Construct a device name using label or manufacturer (vendor and model)
  // details.
  if (name) {
    std::string device_label = GetUdevDevicePropertyValue(device, kLabel);
    if (device_label.empty())
      device_label = GetUdevDevicePropertyValue(device, kSerial);
    if (device_label.empty()) {
      // Format: VendorInfo ModelInfo
      // E.g.: KnCompany Model2010
      device_label = GetUdevDevicePropertyValue(device, kVendor);
      std::string model_name = GetUdevDevicePropertyValue(device, kModel);
      if (device_label.empty())
        device_label = model_name;
      else if (!model_name.empty())
        device_label += kSpaceDelim + model_name;
    }
    if (IsStringUTF8(device_label))
      *name = UTF8ToUTF16(device_label);
  }

  if (unique_id) {
    *unique_id = MakeDeviceUniqueId(device);
  }

  if (removable) {
    const char* value = udev_device_get_sysattr_value(device,
                                                      kRemovableSysAttr);
    if (!value) {
      // |parent_device| is owned by |device| and does not need to be cleaned
      // up.
      struct udev_device* parent_device =
          udev_device_get_parent_with_subsystem_devtype(device,
                                                        kBlockSubsystemKey,
                                                        kDiskDeviceTypeKey);
      value = udev_device_get_sysattr_value(parent_device, kRemovableSysAttr);
    }
    *removable = (value && atoi(value) == 1);
  }
  RecordGetDeviceInfoResult(true);
}

}  // namespace

RemovableDeviceNotificationsLinux::MountPointInfo::MountPointInfo() {
}

RemovableDeviceNotificationsLinux::RemovableDeviceNotificationsLinux(
    const FilePath& path)
    : initialized_(false),
      mtab_path_(path),
      get_device_info_func_(&GetDeviceInfo) {
  DCHECK(!g_removable_device_notifications_linux);
  g_removable_device_notifications_linux = this;
}

RemovableDeviceNotificationsLinux::RemovableDeviceNotificationsLinux(
    const FilePath& path,
    GetDeviceInfoFunc get_device_info_func)
    : initialized_(false),
      mtab_path_(path),
      get_device_info_func_(get_device_info_func) {
  DCHECK(!g_removable_device_notifications_linux);
  g_removable_device_notifications_linux = this;
}

RemovableDeviceNotificationsLinux::~RemovableDeviceNotificationsLinux() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK_EQ(this, g_removable_device_notifications_linux);
  g_removable_device_notifications_linux = NULL;
}

// static
RemovableDeviceNotificationsLinux*
RemovableDeviceNotificationsLinux::GetInstance() {
  DCHECK(g_removable_device_notifications_linux != NULL);
  return g_removable_device_notifications_linux;
}

void RemovableDeviceNotificationsLinux::Init() {
  DCHECK(!mtab_path_.empty());

  // Put |kKnownFileSystems| in std::set to get O(log N) access time.
  for (size_t i = 0; i < arraysize(kKnownFileSystems); ++i)
    known_file_systems_.insert(kKnownFileSystems[i]);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&RemovableDeviceNotificationsLinux::InitOnFileThread, this));
}

FilePath RemovableDeviceNotificationsLinux::GetDeviceMountPoint(
    const std::string& device_id) const {

  MediaStorageUtil::Type type;
  MediaStorageUtil::CrackDeviceId(device_id, &type, NULL);
  if (type == MediaStorageUtil::MTP_OR_PTP)
    return FilePath();
  DCHECK(type == MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM ||
         type == MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM ||
         type == MediaStorageUtil::FIXED_MASS_STORAGE);

  FilePath mount_device;
  for (MountMap::const_iterator it = mount_info_map_.begin();
       it != mount_info_map_.end();
       ++it) {
    if (it->second.device_id == device_id) {
      mount_device = it->second.mount_device;
      break;
    }
  }
  if (mount_device.empty())
    return mount_device;

  const ReferencedMountPoint& referenced_info =
      mount_priority_map_.find(mount_device)->second;
  for (ReferencedMountPoint::const_iterator it = referenced_info.begin();
       it != referenced_info.end();
       ++it) {
    if (it->second)
      return it->first;
  }
  // If none of them are default, just return the first.
  return FilePath(referenced_info.begin()->first);
}

bool RemovableDeviceNotificationsLinux::GetDeviceInfoForPath(
    const FilePath& path,
    SystemMonitor::RemovableStorageInfo* device_info) const {
  if (!path.IsAbsolute())
    return false;

  FilePath current = path;
  while (!ContainsKey(mount_info_map_, current) && current != current.DirName())
    current = current.DirName();

  MountMap::const_iterator mount_info = mount_info_map_.find(current);
  if (mount_info == mount_info_map_.end())
    return false;

  if (device_info) {
    device_info->device_id = mount_info->second.device_id;
    device_info->name = mount_info->second.device_name;
    device_info->location = current.value();
  }
  return true;
}

void RemovableDeviceNotificationsLinux::OnFilePathChanged(const FilePath& path,
                                                          bool error) {
  if (path != mtab_path_) {
    // This cannot happen unless FilePathWatcher is buggy. Just ignore this
    // notification and do nothing.
    NOTREACHED();
    return;
  }
  if (error) {
    LOG(ERROR) << "Error watching " << mtab_path_.value();
    return;
  }

  UpdateMtab();
}

void RemovableDeviceNotificationsLinux::InitOnFileThread() {
  DCHECK(!initialized_);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  initialized_ = true;

  // The callback passed to Watch() has to be unretained. Otherwise
  // RemovableDeviceNotificationsLinux will live longer than expected, and
  // FilePathWatcher will get in trouble at shutdown time.
  bool ret = file_watcher_.Watch(
      mtab_path_,
      base::Bind(&RemovableDeviceNotificationsLinux::OnFilePathChanged,
                 base::Unretained(this)));
  if (!ret) {
    LOG(ERROR) << "Adding watch for " << mtab_path_.value() << " failed";
    return;
  }

  UpdateMtab();
}

void RemovableDeviceNotificationsLinux::UpdateMtab() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  MountPointDeviceMap new_mtab;
  ReadMtab(mtab_path_, known_file_systems_, &new_mtab);

  // Check existing mtab entries for unaccounted mount points.
  // These mount points must have been removed in the new mtab.
  std::list<FilePath> mount_points_to_erase;
  std::list<FilePath> multiple_mounted_devices_needing_reattachment;
  for (MountMap::const_iterator old_iter = mount_info_map_.begin();
       old_iter != mount_info_map_.end(); ++old_iter) {
    const FilePath& mount_point = old_iter->first;
    const FilePath& mount_device = old_iter->second.mount_device;
    MountPointDeviceMap::iterator new_iter = new_mtab.find(mount_point);
    // |mount_point| not in |new_mtab| or |mount_device| is no longer mounted at
    // |mount_point|.
    if (new_iter == new_mtab.end() || (new_iter->second != mount_device)) {
      MountPriorityMap::iterator priority =
          mount_priority_map_.find(mount_device);
      DCHECK(priority != mount_priority_map_.end());
      ReferencedMountPoint::const_iterator has_priority =
          priority->second.find(mount_point);
      if (MediaStorageUtil::IsRemovableDevice(old_iter->second.device_id)) {
        DCHECK(has_priority != priority->second.end());
        if (has_priority->second) {
          SystemMonitor::Get()->ProcessRemovableStorageDetached(
              old_iter->second.device_id);
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
  for (std::list<FilePath>::const_iterator it = mount_points_to_erase.begin();
       it != mount_points_to_erase.end();
       ++it) {
    mount_info_map_.erase(*it);
  }

  // For any multiply mounted device where the mount that we had notified
  // got detached, send a notification of attachment for one of the other
  // mount points.
  for (std::list<FilePath>::const_iterator it =
           multiple_mounted_devices_needing_reattachment.begin();
       it != multiple_mounted_devices_needing_reattachment.end();
       ++it) {
    ReferencedMountPoint::iterator first_mount_point_info =
        mount_priority_map_.find(*it)->second.begin();
    const FilePath& mount_point = first_mount_point_info->first;
    first_mount_point_info->second = true;

    const MountPointInfo& mount_info =
        mount_info_map_.find(mount_point)->second;
    DCHECK(MediaStorageUtil::IsRemovableDevice(mount_info.device_id));
    base::SystemMonitor::Get()->ProcessRemovableStorageAttached(
        mount_info.device_id, mount_info.device_name, mount_point.value());
  }

  // Check new mtab entries against existing ones.
  for (MountPointDeviceMap::iterator new_iter = new_mtab.begin();
       new_iter != new_mtab.end(); ++new_iter) {
    const FilePath& mount_point = new_iter->first;
    const FilePath& mount_device = new_iter->second;
    MountMap::iterator old_iter = mount_info_map_.find(mount_point);
    if (old_iter == mount_info_map_.end() ||
        old_iter->second.mount_device != mount_device) {
      // New mount point found or an existing mount point found with a new
      // device.
      AddNewMount(mount_device, mount_point);
    }
  }
}

void RemovableDeviceNotificationsLinux::AddNewMount(
    const FilePath& mount_device, const FilePath& mount_point) {
  MountPriorityMap::iterator priority =
      mount_priority_map_.find(mount_device);
  if (priority != mount_priority_map_.end()) {
    const FilePath& other_mount_point = priority->second.begin()->first;
    priority->second[mount_point] = false;
    mount_info_map_[mount_point] =
        mount_info_map_.find(other_mount_point)->second;
    return;
  }

  std::string unique_id;
  string16 name;
  bool removable;
  get_device_info_func_(mount_device, &unique_id, &name, &removable);


  // Keep track of device uuid, to see how often we receive empty values.
  UMA_HISTOGRAM_BOOLEAN("MediaDeviceNotification.DeviceUUIDAvailable",
                        !unique_id.empty());
  UMA_HISTOGRAM_BOOLEAN("MediaDeviceNotification.DeviceNameAvailable",
                        !name.empty());
  if (unique_id.empty() || name.empty())
    return;

  bool has_dcim = IsMediaDevice(mount_point.value());
  MediaStorageUtil::Type type;
  if (removable) {
    if (has_dcim) {
      type = MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM;
    } else {
      type = MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM;
    }
  } else {
    type = MediaStorageUtil::FIXED_MASS_STORAGE;
  }
  std::string device_id = MediaStorageUtil::MakeDeviceId(type, unique_id);

  MountPointInfo mount_point_info;
  mount_point_info.mount_device = mount_device;
  mount_point_info.device_id = device_id;
  mount_point_info.device_name = name;

  mount_info_map_[mount_point] = mount_point_info;
  mount_priority_map_[mount_device][mount_point] = removable;

  if (removable) {
    SystemMonitor::Get()->ProcessRemovableStorageAttached(device_id, name,
                                                          mount_point.value());
  }
}

}  // namespace chrome
