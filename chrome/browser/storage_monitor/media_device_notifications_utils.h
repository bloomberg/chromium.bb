// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common code shared between MediaDeviceNotifications{Win,Linux}.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_MEDIA_DEVICE_NOTIFICATIONS_UTILS_H_
#define CHROME_BROWSER_STORAGE_MONITOR_MEDIA_DEVICE_NOTIFICATIONS_UTILS_H_

#include "base/files/file_path.h"
#include "base/string16.h"

namespace chrome {

// Check if the file system at the passed mount point looks like a media
// device using the existence of DCIM directory.
// Returns true, if it is a media device otherwise return false.
// Mac OS X behaves similarly, but this is not the only heuristic it uses.
// TODO(vandebo) Try to figure out how Mac OS X decides this, and rename
// to avoid confusion with MediaStorageUtil::IsMediaDevice.
bool IsMediaDevice(const base::FilePath::StringType& mount_point);

// Constructs the device product name from |vendor_name| and |model_name|.
string16 GetFullProductName(const std::string& vendor_name,
                            const std::string& model_name);

// Constructs the display name for device from |storage_size_in_bytes| and
// |name|.
string16 GetDisplayNameForDevice(uint64 storage_size_in_bytes,
                                 const string16& name);

}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_MEDIA_DEVICE_NOTIFICATIONS_UTILS_H_
