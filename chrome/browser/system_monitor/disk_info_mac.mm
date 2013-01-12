// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/system_monitor/disk_info_mac.h"

#include "base/mac/foundation_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/system_monitor/media_device_notifications_utils.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {
namespace {

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

}  // namespace

DiskInfoMac::DiskInfoMac() : total_size_in_bytes_(0) {
}

DiskInfoMac::~DiskInfoMac() {
}

// static
DiskInfoMac DiskInfoMac::BuildDiskInfoOnFileThread(CFDictionaryRef dict) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  DiskInfoMac info;

  CFStringRef bsd_name = base::mac::GetValueFromDictionary<CFStringRef>(
      dict, kDADiskDescriptionMediaBSDNameKey);
  if (bsd_name)
    info.bsd_name_ = base::SysCFStringRefToUTF8(bsd_name);

  CFURLRef url = base::mac::GetValueFromDictionary<CFURLRef>(
      dict, kDADiskDescriptionVolumePathKey);
  NSURL* nsurl = base::mac::CFToNSCast(url);
  info.mount_point_ = base::mac::NSStringToFilePath([nsurl path]);
  CFNumberRef size_number =
      base::mac::GetValueFromDictionary<CFNumberRef>(
          dict, kDADiskDescriptionMediaSizeKey);
  if (size_number) {
    CFNumberGetValue(size_number, kCFNumberLongLongType,
                     &(info.total_size_in_bytes_));
  }

  string16 vendor_name = GetUTF16FromDictionary(
      dict, kDADiskDescriptionDeviceVendorKey);
  string16 model_name = GetUTF16FromDictionary(
      dict, kDADiskDescriptionDeviceModelKey);
  string16 volume_name = GetUTF16FromDictionary(
      dict, kDADiskDescriptionVolumeNameKey);

  if (!volume_name.empty()) {
    info.device_name_ = volume_name;
  } else {
    info.device_name_ = GetFullProductName(UTF16ToUTF8(vendor_name),
                                           UTF16ToUTF8(model_name));
  }
  info.model_name_ = UTF16ToUTF8(model_name);

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
    string16 unique_id2 = vendor_name;
    unique_id2 = JoinName(unique_id2, model_name);
    unique_id2 = JoinName(unique_id2, revision);
    unique_id = UTF16ToUTF8(unique_id2);
  }

  CFBooleanRef is_removable_ref =
      base::mac::GetValueFromDictionary<CFBooleanRef>(
          dict, kDADiskDescriptionMediaRemovableKey);
  bool is_removable = is_removable_ref && CFBooleanGetValue(is_removable_ref);
  // Checking for DCIM only matters on removable devices.
  bool has_dcim = is_removable && IsMediaDevice(info.mount_point_.value());
  info.type_ = GetDeviceType(is_removable, has_dcim);
  if (!unique_id.empty())
    info.device_id_ = MediaStorageUtil::MakeDeviceId(info.type_, unique_id);

  return info;
}

// TODO(gbillock): Make sure this gets test coverage.
// static
DiskInfoMac DiskInfoMac::BuildDiskInfoFromICDevice(std::string device_id,
                                                   string16 device_name,
                                                   FilePath mount_point) {
  DiskInfoMac info;
  info.device_id_ = device_id;
  info.device_name_ = device_name;
  info.mount_point_ = mount_point;
  info.type_ = MediaStorageUtil::MAC_IMAGE_CAPTURE;
  return info;
}

}  // namesapce chrome
