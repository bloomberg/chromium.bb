// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/storage_monitor_mac.h"

#include "chrome/browser/storage_monitor/media_device_notifications_utils.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

namespace {

const char kDiskImageModelName[] = "Disk Image";

void GetDiskInfoAndUpdateOnFileThread(
    const scoped_refptr<StorageMonitorMac>& monitor,
    base::mac::ScopedCFTypeRef<CFDictionaryRef> dict,
    StorageMonitorMac::UpdateType update_type) {
  DiskInfoMac info = DiskInfoMac::BuildDiskInfoOnFileThread(dict);
  if (info.device_id().empty())
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&StorageMonitorMac::UpdateDisk,
                 monitor,
                 info,
                 update_type));
}

void GetDiskInfoAndUpdate(const scoped_refptr<StorageMonitorMac>& monitor,
                          DADiskRef disk,
                          StorageMonitorMac::UpdateType update_type) {
  base::mac::ScopedCFTypeRef<CFDictionaryRef> dict(DADiskCopyDescription(disk));
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(GetDiskInfoAndUpdateOnFileThread,
                 monitor,
                 dict,
                 update_type));
}

}  // namespace

StorageMonitorMac::StorageMonitorMac() {
  session_.reset(DASessionCreate(NULL));

  DASessionScheduleWithRunLoop(
      session_, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);

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
}

StorageMonitorMac::~StorageMonitorMac() {
  DASessionUnscheduleFromRunLoop(
      session_, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
}

void StorageMonitorMac::UpdateDisk(const DiskInfoMac& info,
                                   UpdateType update_type) {
  if (info.bsd_name().empty())
    return;

  std::map<std::string, DiskInfoMac>::iterator it =
      disk_info_map_.find(info.bsd_name());
  if (it != disk_info_map_.end()) {
    // If an attached notification was previously posted then post a detached
    // notification now. This is used for devices that are being removed or
    // devices that have changed.
    if (ShouldPostNotificationForDisk(it->second)) {
      receiver()->ProcessDetach(it->second.device_id());
    }
  }

  if (update_type == UPDATE_DEVICE_REMOVED) {
    if (it != disk_info_map_.end())
      disk_info_map_.erase(it);
  } else {
    disk_info_map_[info.bsd_name()] = info;
    MediaStorageUtil::RecordDeviceInfoHistogram(true, info.device_id(),
                                                info.device_name());
    if (ShouldPostNotificationForDisk(info)) {
      string16 display_name = GetDisplayNameForDevice(
          info.total_size_in_bytes(), info.device_name());
      receiver()->ProcessAttach(StorageInfo(
          info.device_id(), display_name, info.mount_point().value()));
    }
  }
}

bool StorageMonitorMac::GetStorageInfoForPath(const base::FilePath& path,
                                              StorageInfo* device_info) const {
  if (!path.IsAbsolute())
    return false;

  base::FilePath current = path;
  const base::FilePath root(base::FilePath::kSeparators);
  while (current != root) {
    DiskInfoMac info;
    if (FindDiskWithMountPoint(current, &info)) {
      device_info->device_id = info.device_id();
      device_info->name = info.device_name();
      device_info->location = info.mount_point().value();
      return true;
    }
    current = current.DirName();
  }

  return false;
}

uint64 StorageMonitorMac::GetStorageSize(const std::string& location) const {
  DiskInfoMac info;
  if (!FindDiskWithMountPoint(base::FilePath(location), &info))
    return 0;
  return info.total_size_in_bytes();
}

// static
void StorageMonitorMac::DiskAppearedCallback(DADiskRef disk, void* context) {
  StorageMonitorMac* monitor = static_cast<StorageMonitorMac*>(context);
  GetDiskInfoAndUpdate(monitor, disk, UPDATE_DEVICE_ADDED);
}

// static
void StorageMonitorMac::DiskDisappearedCallback(DADiskRef disk, void* context) {
  StorageMonitorMac* monitor = static_cast<StorageMonitorMac*>(context);
  GetDiskInfoAndUpdate(monitor, disk, UPDATE_DEVICE_REMOVED);
}

// static
void StorageMonitorMac::DiskDescriptionChangedCallback(DADiskRef disk,
                                                       CFArrayRef keys,
                                                       void *context) {
  StorageMonitorMac* monitor = static_cast<StorageMonitorMac*>(context);
  GetDiskInfoAndUpdate(monitor, disk, UPDATE_DEVICE_CHANGED);
}

bool StorageMonitorMac::ShouldPostNotificationForDisk(
    const DiskInfoMac& info) const {
  // Only post notifications about disks that have no empty fields and
  // are removable. Also exclude disk images (DMGs).
  return !info.bsd_name().empty() &&
         !info.device_id().empty() &&
         !info.device_name().empty() &&
         !info.mount_point().empty() &&
         info.model_name() != kDiskImageModelName &&
         (info.type() == MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM ||
          info.type() == MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM);
}

bool StorageMonitorMac::FindDiskWithMountPoint(
    const base::FilePath& mount_point,
    DiskInfoMac* info) const {
  for (std::map<std::string, DiskInfoMac>::const_iterator
      it = disk_info_map_.begin(); it != disk_info_map_.end(); ++it) {
    if (it->second.mount_point() == mount_point) {
      *info = it->second;
      return true;
    }
  }
  return false;
}

}  // namespace chrome
