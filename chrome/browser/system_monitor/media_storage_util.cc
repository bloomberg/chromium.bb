// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chrome::MediaStorageUtil implementation.

#include "chrome/browser/system_monitor/media_storage_util.h"

#include <vector>

#include "base/callback.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/system_monitor/system_monitor.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/system_monitor/media_device_notifications_utils.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/system_monitor/removable_device_notifications_chromeos.h"
#elif defined(OS_LINUX)
#include "chrome/browser/system_monitor/removable_device_notifications_linux.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/system_monitor/removable_device_notifications_mac.h"
#elif defined(OS_WIN)
#include "chrome/browser/system_monitor/removable_device_notifications_window_win.h"
#endif

#if defined(OS_LINUX)  // Implies OS_CHROMEOS
#include "chrome/browser/system_monitor/media_transfer_protocol_device_observer_linux.h"
#endif

using base::SystemMonitor;
using content::BrowserThread;

namespace chrome {

namespace {

typedef std::vector<SystemMonitor::RemovableStorageInfo> RemovableStorageInfo;

// MediaDeviceNotification.DeviceInfo histogram values.
enum DeviceInfoHistogramBuckets {
  MASS_STORAGE_DEVICE_NAME_AND_UUID_AVAILABLE,
  MASS_STORAGE_DEVICE_UUID_MISSING,
  MASS_STORAGE_DEVICE_NAME_MISSING,
  MASS_STORAGE_DEVICE_NAME_AND_UUID_MISSING,
  MTP_STORAGE_DEVICE_NAME_AND_UUID_AVAILABLE,
  MTP_STORAGE_DEVICE_UUID_MISSING,
  MTP_STORAGE_DEVICE_NAME_MISSING,
  MTP_STORAGE_DEVICE_NAME_AND_UUID_MISSING,
  DEVICE_INFO_BUCKET_BOUNDARY
};

// Prefix constants for different device id spaces.
const char kRemovableMassStorageWithDCIMPrefix[] = "dcim:";
const char kRemovableMassStorageNoDCIMPrefix[] = "nodcim:";
const char kFixedMassStoragePrefix[] = "path:";
const char kMtpPtpPrefix[] = "mtp:";

static bool (*g_test_get_device_info_from_path_function)(  // NOLINT
    const FilePath& path, std::string* device_id, string16* device_name,
    FilePath* relative_path) = NULL;

void ValidatePathOnFileThread(
    const FilePath& path, const MediaStorageUtil::BoolCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, file_util::PathExists(path)));
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

void FilterAttachedDevicesOnFileThread(MediaStorageUtil::DeviceIdSet* devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  MediaStorageUtil::DeviceIdSet missing_devices;

  for (MediaStorageUtil::DeviceIdSet::const_iterator it = devices->begin();
       it != devices->end();
       ++it) {
    MediaStorageUtil::Type type;
    std::string unique_id;
    if (!MediaStorageUtil::CrackDeviceId(*it, &type, &unique_id)) {
      missing_devices.insert(*it);
      continue;
    }

    if (type == MediaStorageUtil::FIXED_MASS_STORAGE) {
      if (!file_util::PathExists(FilePath::FromUTF8Unsafe(unique_id)))
        missing_devices.insert(*it);
      continue;
    }

    DCHECK(type == MediaStorageUtil::MTP_OR_PTP ||
           type == MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM ||
           type == MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM);
    if (FindRemovableStorageLocationById(*it).empty())
      missing_devices.insert(*it);
  }

  for (MediaStorageUtil::DeviceIdSet::const_iterator it =
           missing_devices.begin();
       it != missing_devices.end();
       ++it) {
    devices->erase(*it);
  }
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
bool MediaStorageUtil::IsMassStorageDevice(const std::string& device_id) {
  Type type;
  return CrackDeviceId(device_id, &type, NULL) && type != MTP_OR_PTP;
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

  if (type == FIXED_MASS_STORAGE) {
    // For this type, the unique_id is the path.
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            base::Bind(&ValidatePathOnFileThread,
                                       FilePath::FromUTF8Unsafe(unique_id),
                                       callback));
  } else {
    DCHECK(type == MTP_OR_PTP ||
           type == REMOVABLE_MASS_STORAGE_WITH_DCIM ||
           type == REMOVABLE_MASS_STORAGE_NO_DCIM);
    // We should be able to find removable storage in SystemMonitor.
    callback.Run(!FindRemovableStorageLocationById(device_id).empty());
  }
}

// static
void MediaStorageUtil::FilterAttachedDevices(DeviceIdSet* devices,
                                             const base::Closure& done) {
  if (BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    FilterAttachedDevicesOnFileThread(devices);
    done.Run();
    return;
  }
  BrowserThread::PostTaskAndReply(BrowserThread::FILE,
                                  FROM_HERE,
                                  base::Bind(&FilterAttachedDevicesOnFileThread,
                                             devices),
                                  done);
}

bool MediaStorageUtil::GetDeviceInfoFromPath(const FilePath& path,
                                             std::string* device_id,
                                             string16* device_name,
                                             FilePath* relative_path) {
  if (!path.IsAbsolute())
    return false;

  if (g_test_get_device_info_from_path_function) {
    return g_test_get_device_info_from_path_function(path, device_id,
                                                     device_name,
                                                     relative_path);
  }

  bool found_device = false;
  base::SystemMonitor::RemovableStorageInfo device_info;
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
  RemovableDeviceNotifications* notifier =
      RemovableDeviceNotifications::GetInstance();
  found_device = notifier->GetDeviceInfoForPath(path, &device_info);
#endif

#if defined(OS_LINUX)
  if (!found_device) {
    MediaTransferProtocolDeviceObserverLinux* mtp_manager =
        MediaTransferProtocolDeviceObserverLinux::GetInstance();
    found_device = mtp_manager->GetStorageInfoForPath(path, &device_info);
  }
#endif

  if (found_device && IsRemovableDevice(device_info.device_id)) {
    if (device_id)
      *device_id = device_info.device_id;

    FilePath sub_folder_path;
    if (device_name || relative_path) {
      bool success = FilePath(device_info.location)
          .AppendRelativePath(path, &sub_folder_path);
      DCHECK(success);
    }

    if (device_name) {
#if defined(OS_MACOSX) || defined(OS_LINUX)  // Implies OS_CHROMEOS
      *device_name = GetDisplayNameForDevice(
          notifier->GetStorageSize(device_info.location),
          sub_folder_path.value().empty() ?
              device_info.name :
              sub_folder_path.BaseName().LossyDisplayName() +
                  ASCIIToUTF16(" - ") + device_info.name);
#else
      *device_name = device_info.name;
#endif
    }

    if (relative_path)
      *relative_path = sub_folder_path;
    return true;
  }

  // On Posix systems, there's one root so any absolute path could be valid.
#if !defined(OS_POSIX)
  if (!found_device)
    return false;
#endif

  if (device_id)
    *device_id = MakeDeviceId(FIXED_MASS_STORAGE, path.AsUTF8Unsafe());
  if (device_name)
    *device_name = path.BaseName().LossyDisplayName();
  if (relative_path)
    *relative_path = FilePath();
  return true;
}

// static
FilePath MediaStorageUtil::FindDevicePathById(const std::string& device_id) {
  Type type;
  std::string unique_id;
  if (!CrackDeviceId(device_id, &type, &unique_id))
    return FilePath();

  if (type == FIXED_MASS_STORAGE) {
    // For this type, the unique_id is the path.
    return FilePath::FromUTF8Unsafe(unique_id);
  }

  DCHECK(type == MTP_OR_PTP ||
         type == REMOVABLE_MASS_STORAGE_WITH_DCIM ||
         type == REMOVABLE_MASS_STORAGE_NO_DCIM);
  return FilePath(FindRemovableStorageLocationById(device_id));
}

// static
void MediaStorageUtil::RecordDeviceInfoHistogram(bool mass_storage,
                                                 const std::string& device_uuid,
                                                 const string16& device_name) {
  unsigned int event_number = 0;
  if (!mass_storage)
    event_number = 4;

  if (device_name.empty())
    event_number += 2;

  if (device_uuid.empty())
    event_number += 1;
  enum DeviceInfoHistogramBuckets event =
      static_cast<enum DeviceInfoHistogramBuckets>(event_number);
  if (event >= DEVICE_INFO_BUCKET_BOUNDARY) {
    NOTREACHED();
    return;
  }
  UMA_HISTOGRAM_ENUMERATION("MediaDeviceNotifications.DeviceInfo", event,
                            DEVICE_INFO_BUCKET_BOUNDARY);
}

// static
void MediaStorageUtil::SetGetDeviceInfoFromPathFunctionForTesting(
    GetDeviceInfoFromPathFunction function) {
  g_test_get_device_info_from_path_function = function;
}

MediaStorageUtil::MediaStorageUtil() {}

}  // namespace chrome
