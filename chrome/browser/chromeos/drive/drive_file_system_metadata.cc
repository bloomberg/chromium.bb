// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_file_system_metadata.h"

namespace drive {

DriveFileSystemMetadata::DriveFileSystemMetadata()
    : largest_changestamp(0),
      loaded(false),
      refreshing(false),
      push_notification_enabled(false),
      polling_interval_sec(0),
      last_update_check_error(DRIVE_FILE_OK) {
}

DriveFileSystemMetadata::~DriveFileSystemMetadata() {
}

}  // namespace drive
