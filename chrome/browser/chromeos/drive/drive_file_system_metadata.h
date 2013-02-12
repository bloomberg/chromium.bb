// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_METADATA_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_METADATA_H_

#include "base/basictypes.h"
#include "base/time.h"
#include "chrome/browser/chromeos/drive/drive_file_error.h"

namespace drive {

// Metadata of DriveFileSystem. Used by DriveFileSystem::GetMetadata().
struct DriveFileSystemMetadata {
  DriveFileSystemMetadata();
  ~DriveFileSystemMetadata();

  // The largest changestamp that the file system holds (may be different
  // from the one on the server)
  int64 largest_changestamp;

  // True if the file system feed is loaded from the cache or from the server.
  bool loaded;

  // True if the feed is now being fetched from the server.
  bool refreshing;

  // True if push notification is enabled.
  bool push_notification_enabled;

  // Polling interval time in seconds used for fetching delta feeds
  // periodically.
  int polling_interval_sec;

  // Time of the last update check.
  base::Time last_update_check_time;

  // Error code of the last update check.
  DriveFileError last_update_check_error;
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_METADATA_H_
