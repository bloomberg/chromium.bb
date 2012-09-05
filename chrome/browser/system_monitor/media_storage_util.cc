// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chrome::MediaStorageUtil implementation.

#include "chrome/browser/system_monitor/media_storage_util.h"

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

typedef std::vector<SystemMonitor::RemovableStorageInfo> RemovableStorageInfo;

// Prefix constants for different device id spaces.
const char kRemovableMassStorageWithDCIMPrefix[] = "dcim:";
const char kRemovableMassStorageNoDCIMPrefix[] = "nodcim:";
const char kFixedMassStoragePrefix[] = "path:";
const char kMtpPtpPrefix[] = "mtp:";

static void (*g_test_get_device_info_from_path_function)(
    const FilePath& path, std::string* device_id, string16* device_name,
    FilePath* relative_path) = NULL;

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

FilePath::StringType FindRemovableStorageLocationById(
    const std::string& device_id) {
  RemovableStorageInfo media_devices =
      SystemMonitor::Get()->GetAttachedRemovableStorage();
  for (RemovableStorageInfo::const_iterator it = media_devices.begin();
       it != media_devices.end();
       ++it) {
    if (it->device_id == device_id)
      return it->location;
  }
  return FilePath::StringType();
}

}  // namespace

// static
std::string MediaStorageUtil::MakeDeviceId(Type type,
                                           const std::string& unique_id) {
  DCHECK(!unique_id.empty());
  switch (type) {
    case REMOVABLE_MASS_STORAGE_WITH_DCIM:
      return std::string(kRemovableMassStorageWithDCIMPrefix) + unique_id;
    case REMOVABLE_MASS_STORAGE_NO_DCIM:
      return std::string(kRemovableMassStorageNoDCIMPrefix) + unique_id;
    case FIXED_MASS_STORAGE:
      return std::string(kFixedMassStoragePrefix) + unique_id;
    case MTP_OR_PTP:
      return std::string(kMtpPtpPrefix) + unique_id;
  }
  NOTREACHED();
  return std::string();
}

// static
bool MediaStorageUtil::CrackDeviceId(const std::string& device_id,
                                     Type* type, std::string* unique_id) {
  size_t prefix_length = device_id.find_first_of(':');
  std::string prefix = prefix_length != std::string::npos ?
                       device_id.substr(0, prefix_length + 1) : "";

  Type found_type;
  if (prefix == kRemovableMassStorageWithDCIMPrefix) {
    found_type = REMOVABLE_MASS_STORAGE_WITH_DCIM;
  } else if (prefix == kRemovableMassStorageNoDCIMPrefix) {
    found_type = REMOVABLE_MASS_STORAGE_NO_DCIM;
  } else if (prefix == kFixedMassStoragePrefix) {
    found_type = FIXED_MASS_STORAGE;
  } else if (prefix == kMtpPtpPrefix) {
    found_type = MTP_OR_PTP;
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
      (type == REMOVABLE_MASS_STORAGE_WITH_DCIM || type == MTP_OR_PTP);
}

// static
bool MediaStorageUtil::IsRemovableDevice(const std::string& device_id) {
  Type type;
  return CrackDeviceId(device_id, &type, NULL) && type != FIXED_MASS_STORAGE;
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
    case MTP_OR_PTP:  // Fall through.
    case REMOVABLE_MASS_STORAGE_WITH_DCIM: // Fall through.
    case REMOVABLE_MASS_STORAGE_NO_DCIM:
      // We should be able to find removable storage in SystemMonitor.
      callback.Run(!FindRemovableStorageLocationById(device_id).empty());
      break;
    case FIXED_MASS_STORAGE:
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
void MediaStorageUtil::GetDeviceInfoFromPath(const FilePath& path,
                                             std::string* device_id,
                                             string16* device_name,
                                             FilePath* relative_path) {
  if (g_test_get_device_info_from_path_function) {
    g_test_get_device_info_from_path_function(path, device_id, device_name,
                                              relative_path);
  } else {
    GetDeviceInfoFromPathImpl(path, device_id, device_name, relative_path);
  }
}

// static
void MediaStorageUtil::FindDevicePathById(const std::string& device_id,
                                          const FilePathCallback& callback) {
  Type type;
  std::string unique_id;
  if (!CrackDeviceId(device_id, &type, &unique_id))
    callback.Run(FilePath());

  switch (type) {
    case MTP_OR_PTP:
      // TODO(kmadhusu) We may want to return the MTP device location here.
      callback.Run(FilePath());
      break;
    case REMOVABLE_MASS_STORAGE_WITH_DCIM:  // Fall through.
    case REMOVABLE_MASS_STORAGE_NO_DCIM:
      callback.Run(FilePath(FindRemovableStorageLocationById(device_id)));
      break;
    case FIXED_MASS_STORAGE:
      // For this type, the unique_id is the path.
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&ValidatePathOnFileThread,
                     FilePath::FromUTF8Unsafe(unique_id), callback));
      break;
  }
  NOTREACHED();
  callback.Run(FilePath());
}

// static
void MediaStorageUtil::SetGetDeviceInfoFromPathFunctionForTesting(
    GetDeviceInfoFromPathFunction function) {
  g_test_get_device_info_from_path_function = function;
}

MediaStorageUtil::MediaStorageUtil() {}

#if !defined(OS_LINUX) || defined(OS_CHROMEOS)
// static
void MediaStorageUtil::GetDeviceInfoFromPathImpl(const FilePath& path,
                                                 std::string* device_id,
                                                 string16* device_name,
                                                 FilePath* relative_path) {
  // TODO(vandebo) This needs to be implemented per platform.  Below is no
  // worse than what the code already does.
  // * Find mount point parent (determines relative file path)
  // * Search System monitor, just in case.
  // * If it's a removable device, generate device id, else use device root
  //   path as id
  if (device_id)
    *device_id = MakeDeviceId(FIXED_MASS_STORAGE, path.AsUTF8Unsafe());
  if (device_name)
    *device_name = path.BaseName().LossyDisplayName();
  if (relative_path)
    *relative_path = FilePath();
}
#endif

}  // namespace chrome
