// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chrome::MediaStorageUtil implementation.

#include "chrome/browser/media_gallery/media_storage_util.h"

#include <vector>

#include "base/callback.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/system_monitor/system_monitor.h"
#include "content/public/browser/browser_thread.h"

using base::SystemMonitor;
using content::BrowserThread;

namespace chrome {

namespace {

typedef std::vector<SystemMonitor::MediaDeviceInfo> MediaDevicesInfo;

// Prefix constants for different device id spaces.
const char kUsbMassStorageWithDCIMPrefix[] = "dcim:";
const char kUsbMassStorageNoDCIMPrefix[] = "usb:";
const char kOtherMassStoragePrefix[] = "path:";
const char kUsbMtpPrefix[] = "mtp:";

void EmptyPathIsFalseCallback(const MediaStorageUtil::BoolCallback& callback,
                              FilePath path) {
  callback.Run(!path.empty());
}

void ValidatePathOnFileThread(
    const FilePath& path, const MediaStorageUtil::FilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePath result;
  if (file_util::PathExists(path))
    result = path;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, path));
}

FilePath::StringType FindMediaDeviceLocationById(const std::string& device_id) {
  MediaDevicesInfo media_devices =
      SystemMonitor::Get()->GetAttachedMediaDevices();
  for (MediaDevicesInfo::const_iterator it = media_devices.begin();
       it != media_devices.end();
       ++it) {
    if (it->unique_id == device_id)
      return it->location;
  }
  return FilePath::StringType();
}

// TODO(vandebo) use FilePath::AppendRelativePath instead
// Make |path| a relative path, i.e. strip the drive letter and leading /.
FilePath MakePathRelative(const FilePath& path) {
  if (!path.IsAbsolute())
    return path;

  FilePath relative;
  std::vector<FilePath::StringType> components;
  path.GetComponents(&components);

  // On Windows, the first component may be the drive letter with the second
  // being \\.
  int start = 1;
  if (components[1].size() == 1 && FilePath::IsSeparator(components[1][0]))
    start = 2;

  for (size_t i = start; i < components.size(); i++)
    relative = relative.Append(components[i]);
  return relative;
}

}  // namespace

// static
std::string MediaStorageUtil::MakeDeviceId(Type type,
                                           const std::string& unique_id) {
  DCHECK(!unique_id.empty());
  switch (type) {
    case USB_MASS_STORAGE_WITH_DCIM:
      return std::string(kUsbMassStorageWithDCIMPrefix) + unique_id;
    case USB_MASS_STORAGE_NO_DCIM:
      return std::string(kUsbMassStorageNoDCIMPrefix) + unique_id;
    case OTHER_MASS_STORAGE:
      return std::string(kOtherMassStoragePrefix) + unique_id;
    case USB_MTP:
      return std::string(kUsbMtpPrefix) + unique_id;
  }
  NOTREACHED();
  return std::string();
}

// static
bool MediaStorageUtil::CrackDeviceId(const std::string& device_id,
                                     Type* type, std::string* unique_id) {
  size_t prefix_length = device_id.find_first_of(':');
  std::string prefix = device_id.substr(0, prefix_length);

  Type found_type;
  if (prefix == kUsbMassStorageWithDCIMPrefix) {
    found_type = USB_MASS_STORAGE_WITH_DCIM;
  } else if (prefix == kUsbMassStorageNoDCIMPrefix) {
    found_type = USB_MASS_STORAGE_NO_DCIM;
  } else if (prefix == kOtherMassStoragePrefix) {
    found_type = OTHER_MASS_STORAGE;
  } else if (prefix == kUsbMtpPrefix) {
    found_type = USB_MTP;
  } else {
    NOTREACHED();
    return false;
  }
  if (type)
    *type = found_type;

  if (unique_id)
    *unique_id = device_id.substr(prefix_length + 1);
  return true;
}

// static
bool MediaStorageUtil::IsMediaDevice(const std::string& device_id) {
  Type type;
  return CrackDeviceId(device_id, &type, NULL) &&
      (type == USB_MASS_STORAGE_WITH_DCIM || type == USB_MTP);
}

// static
bool MediaStorageUtil::IsRemovableDevice(const std::string& device_id) {
  Type type;
  return CrackDeviceId(device_id, &type, NULL) && type != OTHER_MASS_STORAGE;
}

// static
void MediaStorageUtil::IsDeviceAttached(const std::string& device_id,
                                        const BoolCallback& callback) {
  Type type;
  std::string unique_id;
  if (!CrackDeviceId(device_id, &type, &unique_id)) {
    callback.Run(false);
    return;
  }

  switch (type) {
    case USB_MTP:  // Fall through
    case USB_MASS_STORAGE_WITH_DCIM:
      // We should be able to find media devices in SystemMonitor.
      callback.Run(!FindMediaDeviceLocationById(device_id).empty());
      break;
    case USB_MASS_STORAGE_NO_DCIM:
      FindUSBDeviceById(unique_id,
                        base::Bind(&EmptyPathIsFalseCallback, callback));
      break;
    case OTHER_MASS_STORAGE:
      // For this type, the unique_id is the path.
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&ValidatePathOnFileThread,
                     FilePath::FromUTF8Unsafe(unique_id),
                     base::Bind(&EmptyPathIsFalseCallback, callback)));
      break;
  }
  NOTREACHED();
  callback.Run(false);
}

// static
void MediaStorageUtil::GetDeviceInfoFromPath(
    const FilePath& path, const DeviceInfoCallback& callback) {
  // TODO(vandebo) This needs to be implemented per platform.  Below is no
  // worse than what the code already does.
  // * Find mount point parent (determines relative file path)
  // * Search System monitor, just in case.
  // * If it's a USB device, generate device id, else use device root path as id
  std::string device_id =
      MakeDeviceId(OTHER_MASS_STORAGE, path.AsUTF8Unsafe());
  FilePath relative_path = MakePathRelative(path);
  string16 display_name = path.BaseName().LossyDisplayName();

  callback.Run(device_id, relative_path, display_name);
  return;
}

// static
void MediaStorageUtil::FindDevicePathById(const std::string& device_id,
                                          const FilePathCallback& callback) {
  Type type;
  std::string unique_id;
  if (!CrackDeviceId(device_id, &type, &unique_id))
    callback.Run(FilePath());

  switch (type) {
    case USB_MTP:
      callback.Run(FilePath());
      break;
    case USB_MASS_STORAGE_NO_DCIM:
      FindUSBDeviceById(unique_id, callback);
      break;
    case OTHER_MASS_STORAGE:
      // For this type, the unique_id is the path.
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&ValidatePathOnFileThread,
                     FilePath::FromUTF8Unsafe(unique_id), callback));
      break;
    case USB_MASS_STORAGE_WITH_DCIM:
      callback.Run(FilePath(FindMediaDeviceLocationById(device_id)));
      break;
  }
  NOTREACHED();
  callback.Run(FilePath());
}

MediaStorageUtil::MediaStorageUtil() {}

// static
void MediaStorageUtil::FindUSBDeviceById(const std::string& unique_id,
                                         const FilePathCallback& callback) {
  // TODO(vandebo) This needs to be implemented per platform.
  // Type is USB_MASS_STORAGE_NO_DCIM, so it's a device possibly mounted
  // somewhere...
  NOTREACHED();
  callback.Run(FilePath());
}

}  // namespace chrome
