// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common code shared between MediaDeviceNotifications{Win,Linux}.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_MEDIA_DEVICE_NOTIFICATIONS_UTILS_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_MEDIA_DEVICE_NOTIFICATIONS_UTILS_H_

#include "base/file_path.h"

namespace chrome {

// Check if the file system at the passed mount point looks like a media
// device using the existence of DCIM directory.
// Returns true, if it is a media device otherwise return false.
// Mac OS X behaves similarly, but this is not the only heuristic it uses.
// TODO(vandebo) Try to figure out how Mac OS X decides this.
bool IsMediaDevice(const FilePath::StringType& mount_point);

}  // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_MEDIA_DEVICE_NOTIFICATIONS_UTILS_H_
