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

SyncStatusCode GDataErrorCodeToSyncStatusCode(
    google_apis::GDataErrorCode error) {
  // NOTE: Please update DriveFileSyncService::UpdateServiceState when you add
  // more error code mapping.
  switch (error) {
    case google_apis::HTTP_SUCCESS:
    case google_apis::HTTP_CREATED:
    case google_apis::HTTP_NO_CONTENT:
    case google_apis::HTTP_FOUND:
      return SYNC_STATUS_OK;

    case google_apis::HTTP_NOT_MODIFIED:
      return SYNC_STATUS_NOT_MODIFIED;

    case google_apis::HTTP_CONFLICT:
    case google_apis::HTTP_PRECONDITION:
      return SYNC_STATUS_HAS_CONFLICT;

    case google_apis::HTTP_UNAUTHORIZED:
      return SYNC_STATUS_AUTHENTICATION_FAILED;

    case google_apis::GDATA_NO_CONNECTION:
      return SYNC_STATUS_NETWORK_ERROR;

    case google_apis::HTTP_INTERNAL_SERVER_ERROR:
    case google_apis::HTTP_BAD_GATEWAY:
    case google_apis::HTTP_SERVICE_UNAVAILABLE:
    case google_apis::GDATA_CANCELLED:
    case google_apis::GDATA_NOT_READY:
      return SYNC_STATUS_SERVICE_TEMPORARILY_UNAVAILABLE;

    case google_apis::HTTP_NOT_FOUND:
      return SYNC_FILE_ERROR_NOT_FOUND;

    case google_apis::GDATA_FILE_ERROR:
      return SYNC_FILE_ERROR_FAILED;

    case google_apis::HTTP_FORBIDDEN:
      return SYNC_STATUS_ACCESS_FORBIDDEN;

    case google_apis::HTTP_RESUME_INCOMPLETE:
    case google_apis::HTTP_BAD_REQUEST:
    case google_apis::HTTP_LENGTH_REQUIRED:
    case google_apis::HTTP_NOT_IMPLEMENTED:
    case google_apis::GDATA_PARSE_ERROR:
    case google_apis::GDATA_OTHER_ERROR:
      return SYNC_STATUS_FAILED;

    case google_apis::GDATA_NO_SPACE:
      return SYNC_FILE_ERROR_NO_SPACE;
  }

  // There's a case where DriveService layer returns GDataErrorCode==-1
  // when network is unavailable. (http://crbug.com/223042)
  // TODO(kinuko,nhiroki): We should identify from where this undefined error
  // code is coming.
  if (error == -1)
    return SYNC_STATUS_NETWORK_ERROR;

  util::Log(logging::LOG_WARNING,
            FROM_HERE,
            "Got unexpected error: %d",
            static_cast<int>(error));
  return SYNC_STATUS_FAILED;
}

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
