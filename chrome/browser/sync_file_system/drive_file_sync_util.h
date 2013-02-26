// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_UTIL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_UTIL_H_

#include "chrome/browser/google_apis/drive_upload_error.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

namespace sync_file_system {

// Translates GDataErrorCode to SyncStatusCode.
SyncStatusCode GDataErrorCodeToSyncStatusCode(
    google_apis::GDataErrorCode error);

// Translates DriveUploadError to GDataErrorCode.
google_apis::GDataErrorCode DriveUploadErrorToGDataErrorCode(
    google_apis::DriveUploadError error);

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_FILE_SYNC_UTIL_H_
