// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/system_monitor/media_device_notifications_utils.h"

#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/browser/system_monitor/removable_device_constants.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

bool IsMediaDevice(const FilePath::StringType& mount_point) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  FilePath dcim_path(mount_point);
  FilePath::StringType dcim_dir(kDCIMDirectoryName);
  if (!file_util::DirectoryExists(dcim_path.Append(dcim_dir))) {
    // Check for lowercase 'dcim' as well.
    FilePath dcim_path_lower(dcim_path.Append(StringToLowerASCII(dcim_dir)));
    if (!file_util::DirectoryExists(dcim_path_lower))
      return false;
  }
  return true;
}

}  // namespace chrome
