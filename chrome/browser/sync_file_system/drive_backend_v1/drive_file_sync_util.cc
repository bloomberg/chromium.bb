// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_util.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/sync_file_system/logger.h"

namespace sync_file_system {

namespace {

// A command-line switch to disable Drive API and to make Sync FileSystem API
// work on WAPI (http://crbug.com/234557)
// TODO(nhiroki): this command-line switch should be temporary.
const char kDisableDriveAPI[] = "disable-drive-api-for-syncfs";

bool is_drive_api_disabled = false;

}  // namespace

void SetDisableDriveAPI(bool flag) {
  is_drive_api_disabled = flag;
}

bool IsDriveAPIDisabled() {
  return is_drive_api_disabled ||
      CommandLine::ForCurrentProcess()->HasSwitch(kDisableDriveAPI);
}

ScopedDisableDriveAPI::ScopedDisableDriveAPI()
    : was_disabled_(IsDriveAPIDisabled()) {
  SetDisableDriveAPI(true);
}

ScopedDisableDriveAPI::~ScopedDisableDriveAPI() {
  DCHECK(IsDriveAPIDisabled());
  SetDisableDriveAPI(was_disabled_);
}

}  // namespace sync_file_system
