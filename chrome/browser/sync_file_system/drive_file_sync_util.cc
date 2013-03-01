// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_util.h"

#include "base/logging.h"

namespace sync_file_system {

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
      return SYNC_STATUS_RETRY;

    case google_apis::HTTP_NOT_FOUND:
      return SYNC_FILE_ERROR_NOT_FOUND;

    case google_apis::GDATA_FILE_ERROR:
      return SYNC_FILE_ERROR_FAILED;

    case google_apis::HTTP_RESUME_INCOMPLETE:
    case google_apis::HTTP_BAD_REQUEST:
    case google_apis::HTTP_FORBIDDEN:
    case google_apis::HTTP_LENGTH_REQUIRED:
    case google_apis::HTTP_PRECONDITION:
    case google_apis::GDATA_PARSE_ERROR:
    case google_apis::GDATA_OTHER_ERROR:
      return SYNC_STATUS_FAILED;

    case google_apis::GDATA_NO_SPACE:
      return SYNC_FILE_ERROR_NO_SPACE;
  }
  NOTREACHED();
  return SYNC_STATUS_FAILED;
}

google_apis::GDataErrorCode DriveUploadErrorToGDataErrorCode(
    google_apis::DriveUploadError error) {
  switch (error) {
    case google_apis::DRIVE_UPLOAD_OK:
       return google_apis::HTTP_SUCCESS;
    case google_apis::DRIVE_UPLOAD_ERROR_NOT_FOUND:
      return google_apis::HTTP_NOT_FOUND;
    case google_apis::DRIVE_UPLOAD_ERROR_NO_SPACE:
      return google_apis::GDATA_NO_SPACE;
    case google_apis::DRIVE_UPLOAD_ERROR_CONFLICT:
      return google_apis::HTTP_CONFLICT;
    case google_apis::DRIVE_UPLOAD_ERROR_ABORT:
      return google_apis::GDATA_OTHER_ERROR;
  }
  NOTREACHED();
  return google_apis::GDATA_OTHER_ERROR;
}

}  // namespace sync_file_system
