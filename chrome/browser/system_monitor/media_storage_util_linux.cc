// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Linux specific implementation of chrome::MediaStorageUtil.

#include "chrome/browser/system_monitor/media_storage_util.h"

#include "base/callback.h"
#include "base/system_monitor/system_monitor.h"
#include "chrome/browser/system_monitor/removable_device_notifications_linux.h"

namespace chrome {

// static
void MediaStorageUtil::GetDeviceInfoFromPathImpl(const FilePath& path,
                                                 std::string* device_id,
                                                 string16* device_name,
                                                 FilePath* relative_path) {
  RemovableDeviceNotificationsLinux* device_tracker =
      RemovableDeviceNotificationsLinux::GetInstance();
  DCHECK(device_tracker);
  base::SystemMonitor::RemovableStorageInfo device_info;
  bool found_device = device_tracker->GetDeviceInfoForPath(path, &device_info);

  if (found_device && IsRemovableDevice(device_info.device_id)) {
    if (device_id)
      *device_id = device_info.device_id;
    if (device_name)
      *device_name = device_info.name;
    if (relative_path) {
      *relative_path = FilePath();
      FilePath mount_point(device_info.location);
      mount_point.AppendRelativePath(path, relative_path);
    }
    return;
  }

  if (device_id)
    *device_id = MakeDeviceId(FIXED_MASS_STORAGE, path.AsUTF8Unsafe());
  if (device_name)
    *device_name = path.BaseName().LossyDisplayName();
  if (relative_path)
    *relative_path = FilePath();
}

}  // namespace chrome
