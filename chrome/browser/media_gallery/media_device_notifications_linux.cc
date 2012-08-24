// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaDeviceNotificationsLinux implementation.

#include "chrome/browser/media_gallery/media_device_notifications_linux.h"

#include <libudev.h>
#include <mntent.h>
#include <stdio.h>

#include <vector>

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
#include "chrome/browser/media_gallery/media_device_notifications_utils.h"
#include "chrome/browser/media_gallery/media_gallery_constants.h"
#include "chrome/browser/media_gallery/media_storage_util.h"

namespace chrome {

using base::SystemMonitor;
using content::BrowserThread;

namespace {

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
const char kDevName[] = "DEVNAME";
const char kFsUUID[] = "ID_FS_UUID";
const char kLabel[] = "ID_FS_LABEL";
const char kModel[] = "ID_MODEL";
const char kModelID[] = "ID_MODEL_ID";
const char kSerial[] = "ID_SERIAL";
const char kSerialShort[] = "ID_SERIAL_SHORT";
const char kVendor[] = "ID_VENDOR";
const char kVendorID[] = "ID_VENDOR_ID";

// Device mount point details.
struct MountPointEntryInfo {
  std::string mount_point;
  int entry_pos;
};

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

// Get the device information using udev library.
// On success, returns true and fill in |id| and |name|.
bool GetDeviceInfo(const std::string& device_path, std::string* id,
                   string16* name) {
  DCHECK(!device_path.empty());

  ScopedUdevObject udev_obj(udev_new());
  if (!udev_obj.get())
    return false;

  struct stat device_stat;
  if (stat(device_path.c_str(), &device_stat) < 0)
    return false;

  char device_type;
  if (S_ISCHR(device_stat.st_mode))
    device_type = 'c';
  else if (S_ISBLK(device_stat.st_mode))
    device_type = 'b';
  else
    return false;  // Not a supported type.

  ScopedUdevDeviceObject device(
      udev_device_new_from_devnum(udev_obj, device_type, device_stat.st_rdev));
  if (!device.get())
    return false;

  // Construct a device name using label or manufacturer (vendor and model)
  // details.
  if (name) {
    std::string device_label = GetUdevDevicePropertyValue(device, kLabel);
    if (device_label.empty())
      device_label = udev_device_get_property_value(device, kSerial);
    if (device_label.empty()) {
      // Format: VendorInfo ModelInfo
      // E.g.: KnCompany Model2010
      std::string vendor_name = GetUdevDevicePropertyValue(device, kVendor);
      std::string model_name = GetUdevDevicePropertyValue(device, kModel);
      if (vendor_name.empty() && model_name.empty())
        return false;

      if (vendor_name.empty())
        device_label = model_name;
      else if (model_name.empty())
        device_label = vendor_name;
      else
        device_label = vendor_name + kSpaceDelim + model_name;
    }

    if (IsStringUTF8(device_label))
      *name = UTF8ToUTF16(device_label);
  }

  // Construct a device id using label or manufacturer (vendor and model)
  // details.
  if (id) {
    std::string unique_id = GetUdevDevicePropertyValue(device, kFsUUID);
    if (unique_id.empty()) {
      // If one of the vendor, model, serial information is missing, its value
      // in the string is empty.
      // Format: VendorModelSerial:VendorInfo:ModelInfo:SerialShortInfo
      // E.g.: VendorModelSerial:Kn:DataTravel_12.10:8000000000006CB02CDB
      std::string vendor = GetUdevDevicePropertyValue(device, kVendorID);
      std::string model = GetUdevDevicePropertyValue(device, kModelID);
      std::string serial_short = GetUdevDevicePropertyValue(device,
                                                            kSerialShort);
      if (vendor.empty() && model.empty() && serial_short.empty())
        return false;

      unique_id = base::StringPrintf("%s%s%s%s%s%s",
                                     kVendorModelSerialPrefix,
                                     vendor.c_str(),
                                     kNonSpaceDelim,
                                     model.c_str(),
                                     kNonSpaceDelim,
                                     serial_short.c_str());
    } else {
      unique_id = kFSUniqueIdPrefix + unique_id;
    }
    *id = MediaStorageUtil::MakeDeviceId(
        MediaStorageUtil::USB_MASS_STORAGE_WITH_DCIM, unique_id);
  }
  return true;
}

}  // namespace

MediaDeviceNotificationsLinux::MediaDeviceNotificationsLinux(
    const FilePath& path)
    : mtab_path_(path),
      get_device_info_func_(&GetDeviceInfo) {
  Init();
}

MediaDeviceNotificationsLinux::MediaDeviceNotificationsLinux(
    const FilePath& path,
    GetDeviceInfoFunc get_device_info_func)
    : mtab_path_(path),
      get_device_info_func_(get_device_info_func) {
  Init();
}

MediaDeviceNotificationsLinux::~MediaDeviceNotificationsLinux() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

void MediaDeviceNotificationsLinux::OnFilePathChanged(const FilePath& path,
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

void MediaDeviceNotificationsLinux::Init() {
  DCHECK(!mtab_path_.empty());

  // Put |kKnownFileSystems| in std::set to get O(log N) access time.
  for (size_t i = 0; i < arraysize(kKnownFileSystems); ++i)
    known_file_systems_.insert(kKnownFileSystems[i]);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MediaDeviceNotificationsLinux::InitOnFileThread, this));
}

void MediaDeviceNotificationsLinux::InitOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // The callback passed to Watch() has to be unretained. Otherwise
  // MediaDeviceNotificationsLinux will live longer than expected, and
  // FilePathWatcher will get in trouble at shutdown time.
  bool ret = file_watcher_.Watch(
      mtab_path_,
      base::Bind(&MediaDeviceNotificationsLinux::OnFilePathChanged,
                 base::Unretained(this)));
  if (!ret) {
    LOG(ERROR) << "Adding watch for " << mtab_path_.value() << " failed";
    return;
  }

  UpdateMtab();
}

void MediaDeviceNotificationsLinux::UpdateMtab() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  MountPointDeviceMap new_mtab;
  ReadMtab(&new_mtab);

  // Check existing mtab entries for unaccounted mount points.
  // These mount points must have been removed in the new mtab.
  std::vector<std::string> mount_points_to_erase;
  for (MountMap::const_iterator old_iter = mount_info_map_.begin();
       old_iter != mount_info_map_.end(); ++old_iter) {
    const std::string& mount_point = old_iter->first;
    const std::string& mount_device = old_iter->second.mount_device;
    MountPointDeviceMap::iterator new_iter = new_mtab.find(mount_point);
    // |mount_point| not in |new_mtab| or |mount_device| is no longer mounted at
    // |mount_point|.
    if (new_iter == new_mtab.end() || (new_iter->second != mount_device)) {
      RemoveOldDevice(old_iter->second.device_id);
      mount_points_to_erase.push_back(mount_point);
    }
  }
  // Erase the |mount_info_map_| entries afterwards. Erasing in the loop above
  // using the iterator is slightly more efficient, but more tricky, since
  // calling std::map::erase() on an iterator invalidates it.
  for (size_t i = 0; i < mount_points_to_erase.size(); ++i)
    mount_info_map_.erase(mount_points_to_erase[i]);

  // Check new mtab entries against existing ones.
  for (MountPointDeviceMap::iterator new_iter = new_mtab.begin();
       new_iter != new_mtab.end(); ++new_iter) {
    const std::string& mount_point = new_iter->first;
    const std::string& mount_device = new_iter->second;
    MountMap::iterator old_iter = mount_info_map_.find(mount_point);
    if (old_iter == mount_info_map_.end() ||
        old_iter->second.mount_device != mount_device) {
      // New mount point found or an existing mount point found with a new
      // device.
      CheckAndAddMediaDevice(mount_device, mount_point);
    }
  }
}

void MediaDeviceNotificationsLinux::ReadMtab(MountPointDeviceMap* mtab) {
  FILE* fp = setmntent(mtab_path_.value().c_str(), "r");
  if (!fp)
    return;

  mntent entry;
  char buf[512];

  // Keep track of mount point entry positions in mtab file.
  int entry_pos = 0;

  // Helper map to store the device mount point details.
  // (mount device, MountPointEntryInfo)
  typedef std::map<std::string, MountPointEntryInfo> DeviceMap;
  DeviceMap device_map;
  while (getmntent_r(fp, &entry, buf, sizeof(buf))) {
    // We only care about real file systems.
    if (!ContainsKey(known_file_systems_, entry.mnt_type))
      continue;

    // Add entries, but overwrite entries for the same mount device. Keep track
    // of the entry positions in |entry_info| and use that below to resolve
    // multiple devices mounted at the same mount point.
    MountPointEntryInfo entry_info;
    entry_info.mount_point = entry.mnt_dir;
    entry_info.entry_pos = entry_pos++;
    device_map[entry.mnt_fsname] = entry_info;
  }
  endmntent(fp);

  // Helper map to store mount point entries.
  // (mount point, entry position in mtab file)
  typedef std::map<std::string, int> MountPointsInfoMap;
  MountPointsInfoMap mount_points_info_map;
  MountPointDeviceMap& new_mtab = *mtab;
  for (DeviceMap::const_iterator device_it = device_map.begin();
       device_it != device_map.end();
       ++device_it) {
    // Add entries, but overwrite entries for the same mount point. Keep track
    // of the entry positions and use that information to resolve multiple
    // devices mounted at the same mount point.
    const std::string& mount_device = device_it->first;
    const std::string& mount_point = device_it->second.mount_point;
    const int entry_pos = device_it->second.entry_pos;
    MountPointDeviceMap::iterator new_it = new_mtab.find(mount_point);
    MountPointsInfoMap::iterator mount_point_info_map_it =
        mount_points_info_map.find(mount_point);

    // New mount point entry found or there is already a device mounted at
    // |mount_point| and the existing mount entry is older than the current one.
    if (new_it == new_mtab.end() ||
        (mount_point_info_map_it != mount_points_info_map.end() &&
         mount_point_info_map_it->second < entry_pos)) {
      new_mtab[mount_point] = mount_device;
      mount_points_info_map[mount_point] = entry_pos;
    }
  }
}

void MediaDeviceNotificationsLinux::CheckAndAddMediaDevice(
    const std::string& mount_device,
    const std::string& mount_point) {
  if (!IsMediaDevice(mount_point))
    return;

  std::string id;
  string16 name;
  bool result = (*get_device_info_func_)(mount_device, &id, &name);

  // Keep track of GetDeviceInfo result, to see how often we fail to get device
  // details.
  UMA_HISTOGRAM_BOOLEAN("MediaDeviceNotification.device_info_available",
                        result);
  if (!result)
    return;

  // Keep track of device uuid, to see how often we receive empty values.
  UMA_HISTOGRAM_BOOLEAN("MediaDeviceNotification.device_uuid_available",
                        !id.empty());
  UMA_HISTOGRAM_BOOLEAN("MediaDeviceNotification.device_name_available",
                        !name.empty());

  if (id.empty() || name.empty())
    return;

  MountDeviceAndId mount_device_and_id;
  mount_device_and_id.mount_device = mount_device;
  mount_device_and_id.device_id = id;
  mount_info_map_[mount_point] = mount_device_and_id;

  SystemMonitor::Get()->ProcessMediaDeviceAttached(id, name, mount_point);
}

void MediaDeviceNotificationsLinux::RemoveOldDevice(
    const std::string& device_id) {
  SystemMonitor::Get()->ProcessMediaDeviceDetached(device_id);
}

}  // namespace chrome
