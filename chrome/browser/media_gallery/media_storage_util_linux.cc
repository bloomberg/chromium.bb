// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Linux specific implementation of chrome::MediaStorageUtil.

#include "chrome/browser/media_gallery/media_storage_util.h"

#include "base/callback.h"
#include "base/system_monitor/system_monitor.h"
#include "chrome/browser/media_gallery/removable_device_notifications_linux.h"

namespace chrome {

// static
void MediaStorageUtil::GetDeviceInfoFromPath(
    const FilePath& path, const DeviceInfoCallback& callback) {
  RemovableDeviceNotificationsLinux* device_tracker =
      RemovableDeviceNotificationsLinux::GetInstance();
  DCHECK(device_tracker);
  base::SystemMonitor::RemovableStorageInfo device_info;
  bool found_device = device_tracker->GetDeviceInfoForPath(path, &device_info);

  FilePath relative_path;
  if (found_device && IsRemovableDevice(device_info.device_id)) {
    FilePath mount_point(device_info.location);
    mount_point.AppendRelativePath(path, &relative_path);
  } else {
    device_info.device_id =
        MakeDeviceId(FIXED_MASS_STORAGE, path.AsUTF8Unsafe());
    device_info.name = path.BaseName().LossyDisplayName();
  }
  callback.Run(device_info.device_id, relative_path, device_info.name);
}

}  // namespace chrome
