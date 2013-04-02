// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/storage_monitor_mac.h"

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/image_capture_device_manager.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

namespace {

const char kDiskImageModelName[] = "Disk Image";

string16 GetUTF16FromDictionary(CFDictionaryRef dictionary, CFStringRef key) {
  CFStringRef value =
      base::mac::GetValueFromDictionary<CFStringRef>(dictionary, key);
  if (!value)
    return string16();
  return base::SysCFStringRefToUTF16(value);
}

string16 JoinName(const string16& name, const string16& addition) {
  if (addition.empty())
    return name;
  if (name.empty())
    return addition;
  return name + static_cast<char16>(' ') + addition;
}

MediaStorageUtil::Type GetDeviceType(bool is_removable, bool has_dcim) {
  if (!is_removable)
    return MediaStorageUtil::FIXED_MASS_STORAGE;
  if (has_dcim)
    return MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM;
  return MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM;
}

StorageInfo BuildStorageInfo(
    CFDictionaryRef dict, std::string* bsd_name) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  StorageInfo info;

  CFStringRef device_bsd_name = base::mac::GetValueFromDictionary<CFStringRef>(
      dict, kDADiskDescriptionMediaBSDNameKey);
  if (device_bsd_name && bsd_name)
    *bsd_name = base::SysCFStringRefToUTF8(device_bsd_name);

  CFURLRef url = base::mac::GetValueFromDictionary<CFURLRef>(
      dict, kDADiskDescriptionVolumePathKey);
  NSURL* nsurl = base::mac::CFToNSCast(url);
  info.location = base::mac::NSStringToFilePath([nsurl path]).value();
  CFNumberRef size_number =
      base::mac::GetValueFromDictionary<CFNumberRef>(
          dict, kDADiskDescriptionMediaSizeKey);
  if (size_number) {
    CFNumberGetValue(size_number, kCFNumberLongLongType,
                     &(info.total_size_in_bytes));
  }

  info.vendor_name = GetUTF16FromDictionary(
      dict, kDADiskDescriptionDeviceVendorKey);
  info.model_name = GetUTF16FromDictionary(
      dict, kDADiskDescriptionDeviceModelKey);
  info.storage_label = GetUTF16FromDictionary(
      dict, kDADiskDescriptionVolumeNameKey);

  if (!info.storage_label.empty()) {
    info.name = info.storage_label;
  } else {
    info.name = MediaStorageUtil::GetFullProductName(
        UTF16ToUTF8(info.vendor_name), UTF16ToUTF8(info.model_name));
  }

  CFUUIDRef uuid = base::mac::GetValueFromDictionary<CFUUIDRef>(
      dict, kDADiskDescriptionVolumeUUIDKey);
  std::string unique_id;
  if (uuid) {
    base::mac::ScopedCFTypeRef<CFStringRef> uuid_string(
        CFUUIDCreateString(NULL, uuid));
    if (uuid_string.get())
      unique_id = base::SysCFStringRefToUTF8(uuid_string);
  }
  if (unique_id.empty()) {
    string16 revision = GetUTF16FromDictionary(
        dict, kDADiskDescriptionDeviceRevisionKey);
    string16 unique_id2 = info.vendor_name;
    unique_id2 = JoinName(unique_id2, info.model_name);
    unique_id2 = JoinName(unique_id2, revision);
    unique_id = UTF16ToUTF8(unique_id2);
  }

  CFBooleanRef is_removable_ref =
      base::mac::GetValueFromDictionary<CFBooleanRef>(
          dict, kDADiskDescriptionMediaRemovableKey);
  bool is_removable = is_removable_ref && CFBooleanGetValue(is_removable_ref);
  // Checking for DCIM only matters on removable devices.
  bool has_dcim = is_removable &&
                  MediaStorageUtil::IsMediaDevice(info.location);
  MediaStorageUtil::Type device_type = GetDeviceType(is_removable, has_dcim);
  if (!unique_id.empty())
    info.device_id = MediaStorageUtil::MakeDeviceId(device_type,
                                                    unique_id);

  return info;
}

void GetDiskInfoAndUpdateOnFileThread(
    const base::WeakPtr<StorageMonitorMac>& monitor,
    base::mac::ScopedCFTypeRef<CFDictionaryRef> dict,
    StorageMonitorMac::UpdateType update_type) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  std::string bsd_name;
  StorageInfo info = BuildStorageInfo(dict, &bsd_name);

  if (info.device_id.empty())
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&StorageMonitorMac::UpdateDisk,
                 monitor,
                 bsd_name,
                 info,
                 update_type));
}

void GetDiskInfoAndUpdate(StorageMonitorMac* monitor,
                          DADiskRef disk,
                          StorageMonitorMac::UpdateType update_type) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  base::mac::ScopedCFTypeRef<CFDictionaryRef> dict(DADiskCopyDescription(disk));
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(GetDiskInfoAndUpdateOnFileThread,
                 monitor->AsWeakPtr(),
                 dict,
                 update_type));
}

struct EjectDiskOptions {
  std::string bsd_name;
  base::Callback<void(StorageMonitor::EjectStatus)> callback;
  base::mac::ScopedCFTypeRef<DADiskRef> disk;
};

void PostEjectCallback(DADiskRef disk,
                       DADissenterRef dissenter,
                       void* context) {
  scoped_ptr<EjectDiskOptions> options_deleter(
      static_cast<EjectDiskOptions*>(context));
  if (dissenter) {
    options_deleter->callback.Run(StorageMonitor::EJECT_IN_USE);
    return;
  }

  options_deleter->callback.Run(StorageMonitor::EJECT_OK);
}

void PostUnmountCallback(DADiskRef disk,
                         DADissenterRef dissenter,
                         void* context) {
  scoped_ptr<EjectDiskOptions> options_deleter(
      static_cast<EjectDiskOptions*>(context));
  if (dissenter) {
    options_deleter->callback.Run(StorageMonitor::EJECT_IN_USE);
    return;
  }

  DADiskEject(options_deleter->disk.get(), kDADiskEjectOptionDefault,
              PostEjectCallback, options_deleter.release());
}

void EjectDisk(EjectDiskOptions* options) {
  DADiskUnmount(options->disk.get(), kDADiskUnmountOptionWhole,
                PostUnmountCallback, options);
}

}  // namespace

StorageMonitorMac::StorageMonitorMac() {
}

StorageMonitorMac::~StorageMonitorMac() {
  if (session_.get()) {
    DASessionUnscheduleFromRunLoop(
        session_, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
  }
}

void StorageMonitorMac::Init() {
  session_.reset(DASessionCreate(NULL));

  // Register for callbacks for attached, changed, and removed devices.
  // This will send notifications for existing devices too.
  DARegisterDiskAppearedCallback(
      session_,
      kDADiskDescriptionMatchVolumeMountable,
      DiskAppearedCallback,
      this);
  DARegisterDiskDisappearedCallback(
      session_,
      kDADiskDescriptionMatchVolumeMountable,
      DiskDisappearedCallback,
      this);
  DARegisterDiskDescriptionChangedCallback(
      session_,
      kDADiskDescriptionMatchVolumeMountable,
      kDADiskDescriptionWatchVolumePath,
      DiskDescriptionChangedCallback,
      this);

  DASessionScheduleWithRunLoop(
      session_, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);

  if (base::mac::IsOSLionOrLater()) {
    image_capture_device_manager_.reset(new chrome::ImageCaptureDeviceManager);
    image_capture_device_manager_->SetNotifications(receiver());
  }
}

void StorageMonitorMac::UpdateDisk(
    const std::string& bsd_name,
    const StorageInfo& info,
    UpdateType update_type) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (bsd_name.empty())
    return;

  std::map<std::string, StorageInfo>::iterator it =
      disk_info_map_.find(bsd_name);
  if (it != disk_info_map_.end()) {
    // If an attached notification was previously posted then post a detached
    // notification now. This is used for devices that are being removed or
    // devices that have changed.
    if (ShouldPostNotificationForDisk(it->second)) {
      receiver()->ProcessDetach(it->second.device_id);
    }
  }

  StorageInfo storage_info(info);
  if (ShouldPostNotificationForDisk(storage_info)) {
    storage_info.name = MediaStorageUtil::GetDisplayNameForDevice(
        storage_info.total_size_in_bytes, storage_info.name);
  }

  if (update_type == UPDATE_DEVICE_REMOVED) {
    if (it != disk_info_map_.end())
      disk_info_map_.erase(it);
  } else {
    disk_info_map_[bsd_name] = storage_info;
    MediaStorageUtil::RecordDeviceInfoHistogram(true, storage_info.device_id,
                                                storage_info.name);
    if (ShouldPostNotificationForDisk(storage_info))
      receiver()->ProcessAttach(storage_info);
  }
}

bool StorageMonitorMac::GetStorageInfoForPath(const base::FilePath& path,
                                              StorageInfo* device_info) const {
  if (!path.IsAbsolute())
    return false;

  base::FilePath current = path;
  const base::FilePath root(base::FilePath::kSeparators);
  while (current != root) {
    StorageInfo info;
    if (FindDiskWithMountPoint(current, &info)) {
      *device_info = info;
      return true;
    }
    current = current.DirName();
  }

  return false;
}

uint64 StorageMonitorMac::GetStorageSize(const std::string& location) const {
  StorageInfo info;
  if (!FindDiskWithMountPoint(base::FilePath(location), &info))
    return 0;
  return info.total_size_in_bytes;
}

void StorageMonitorMac::EjectDevice(
      const std::string& device_id,
      base::Callback<void(EjectStatus)> callback) {
  std::string bsd_name;
  for (std::map<std::string, StorageInfo>::iterator
      it = disk_info_map_.begin(); it != disk_info_map_.end(); ++it) {
    if (it->second.device_id == device_id) {
      bsd_name = it->first;
      disk_info_map_.erase(it);
      break;
    }
  }

  if (bsd_name.empty()) {
    callback.Run(EJECT_NO_SUCH_DEVICE);
    return;
  }

  receiver()->ProcessDetach(device_id);

  base::mac::ScopedCFTypeRef<DADiskRef> disk(
      DADiskCreateFromBSDName(NULL, session_, bsd_name.c_str()));
  if (!disk.get()) {
    callback.Run(StorageMonitor::EJECT_FAILURE);
    return;
  }
  // Get the reference to the full disk for ejecting.
  disk.reset(DADiskCopyWholeDisk(disk));
  if (!disk.get()) {
    callback.Run(StorageMonitor::EJECT_FAILURE);
    return;
  }

  EjectDiskOptions* options = new EjectDiskOptions;
  options->bsd_name = bsd_name;
  options->callback = callback;
  options->disk.reset(disk.release());
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(EjectDisk, options));
}

// static
void StorageMonitorMac::DiskAppearedCallback(DADiskRef disk, void* context) {
  StorageMonitorMac* monitor = static_cast<StorageMonitorMac*>(context);
  GetDiskInfoAndUpdate(monitor->AsWeakPtr(), disk, UPDATE_DEVICE_ADDED);
}

// static
void StorageMonitorMac::DiskDisappearedCallback(DADiskRef disk, void* context) {
  StorageMonitorMac* monitor = static_cast<StorageMonitorMac*>(context);
  GetDiskInfoAndUpdate(monitor->AsWeakPtr(), disk, UPDATE_DEVICE_REMOVED);
}

// static
void StorageMonitorMac::DiskDescriptionChangedCallback(DADiskRef disk,
                                                       CFArrayRef keys,
                                                       void *context) {
  StorageMonitorMac* monitor = static_cast<StorageMonitorMac*>(context);
  GetDiskInfoAndUpdate(monitor->AsWeakPtr(), disk, UPDATE_DEVICE_CHANGED);
}

bool StorageMonitorMac::ShouldPostNotificationForDisk(
    const StorageInfo& info) const {
  // Only post notifications about disks that have no empty fields and
  // are removable. Also exclude disk images (DMGs).
  return !info.device_id.empty() &&
         !info.name.empty() &&
         !info.location.empty() &&
         info.model_name != ASCIIToUTF16(kDiskImageModelName) &&
         MediaStorageUtil::IsRemovableDevice(info.device_id) &&
         MediaStorageUtil::IsMassStorageDevice(info.device_id);
}

bool StorageMonitorMac::FindDiskWithMountPoint(
    const base::FilePath& mount_point,
    StorageInfo* info) const {
  for (std::map<std::string, StorageInfo>::const_iterator
      it = disk_info_map_.begin(); it != disk_info_map_.end(); ++it) {
    if (it->second.location == mount_point.value()) {
      *info = it->second;
      return true;
    }
  }
  return false;
}

}  // namespace chrome
