// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_file_error.h"

#include "base/logging.h"

namespace drive {

std::string DriveFileErrorToString(DriveFileError error) {
  switch (error) {
    case DRIVE_FILE_OK:
      return "DRIVE_FILE_OK";

    case DRIVE_FILE_ERROR_FAILED:
      return "DRIVE_FILE_ERROR_FAILED";

    case DRIVE_FILE_ERROR_IN_USE:
      return "DRIVE_FILE_ERROR_IN_USE";

    case DRIVE_FILE_ERROR_EXISTS:
      return "DRIVE_FILE_ERROR_EXISTS";

    case DRIVE_FILE_ERROR_NOT_FOUND:
      return "DRIVE_FILE_ERROR_NOT_FOUND";

    case DRIVE_FILE_ERROR_ACCESS_DENIED:
      return "DRIVE_FILE_ERROR_ACCESS_DENIED";

    case DRIVE_FILE_ERROR_TOO_MANY_OPENED:
      return "DRIVE_FILE_ERROR_TOO_MANY_OPENED";

    case DRIVE_FILE_ERROR_NO_MEMORY:
      return "DRIVE_FILE_ERROR_NO_MEMORY";

    case DRIVE_FILE_ERROR_NO_SPACE:
      return "DRIVE_FILE_ERROR_NO_SPACE";

    case DRIVE_FILE_ERROR_NOT_A_DIRECTORY:
      return "DRIVE_FILE_ERROR_NOT_A_DIRECTORY";

    case DRIVE_FILE_ERROR_INVALID_OPERATION:
      return "DRIVE_FILE_ERROR_INVALID_OPERATION";

    case DRIVE_FILE_ERROR_SECURITY:
      return "DRIVE_FILE_ERROR_SECURITY";

    case DRIVE_FILE_ERROR_ABORT:
      return "DRIVE_FILE_ERROR_ABORT";

    case DRIVE_FILE_ERROR_NOT_A_FILE:
      return "DRIVE_FILE_ERROR_NOT_A_FILE";

    case DRIVE_FILE_ERROR_NOT_EMPTY:
      return "DRIVE_FILE_ERROR_NOT_EMPTY";

    case DRIVE_FILE_ERROR_INVALID_URL:
      return "DRIVE_FILE_ERROR_INVALID_URL";

    case DRIVE_FILE_ERROR_NO_CONNECTION:
      return "DRIVE_FILE_ERROR_NO_CONNECTION";

    case DRIVE_FILE_ERROR_THROTTLED:
      return "DRIVE_FILE_ERROR_THROTTLED";
  }

  NOTREACHED();
  return "";
}

base::PlatformFileError DriveFileErrorToPlatformError(DriveFileError error) {
  switch (error) {
    case DRIVE_FILE_OK:
      return base::PLATFORM_FILE_OK;

    case DRIVE_FILE_ERROR_FAILED:
      return base::PLATFORM_FILE_ERROR_FAILED;

    case DRIVE_FILE_ERROR_IN_USE:
      return base::PLATFORM_FILE_ERROR_IN_USE;

    case DRIVE_FILE_ERROR_EXISTS:
      return base::PLATFORM_FILE_ERROR_EXISTS;

    case DRIVE_FILE_ERROR_NOT_FOUND:
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;

    case DRIVE_FILE_ERROR_ACCESS_DENIED:
      return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;

    case DRIVE_FILE_ERROR_TOO_MANY_OPENED:
      return base::PLATFORM_FILE_ERROR_TOO_MANY_OPENED;

    case DRIVE_FILE_ERROR_NO_MEMORY:
      return base::PLATFORM_FILE_ERROR_NO_MEMORY;

    case DRIVE_FILE_ERROR_NO_SPACE:
      return base::PLATFORM_FILE_ERROR_NO_SPACE;

    case DRIVE_FILE_ERROR_NOT_A_DIRECTORY:
      return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;

    case DRIVE_FILE_ERROR_INVALID_OPERATION:
      return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

    case DRIVE_FILE_ERROR_SECURITY:
      return base::PLATFORM_FILE_ERROR_SECURITY;

    case DRIVE_FILE_ERROR_ABORT:
      return base::PLATFORM_FILE_ERROR_ABORT;

    case DRIVE_FILE_ERROR_NOT_A_FILE:
      return base::PLATFORM_FILE_ERROR_NOT_A_FILE;

    case DRIVE_FILE_ERROR_NOT_EMPTY:
      return base::PLATFORM_FILE_ERROR_NOT_EMPTY;

    case DRIVE_FILE_ERROR_INVALID_URL:
      return base::PLATFORM_FILE_ERROR_INVALID_URL;

    case DRIVE_FILE_ERROR_NO_CONNECTION:
      return base::PLATFORM_FILE_ERROR_FAILED;

    case DRIVE_FILE_ERROR_THROTTLED:
      return base::PLATFORM_FILE_ERROR_FAILED;
  }

  NOTREACHED();
  return base::PLATFORM_FILE_ERROR_FAILED;
}

DriveFileError DriveUploadErrorToDriveFileError(
    google_apis::DriveUploadError error) {
  switch (error) {
    case google_apis::DRIVE_UPLOAD_OK:
      return DRIVE_FILE_OK;

    case google_apis::DRIVE_UPLOAD_ERROR_NOT_FOUND:
      return DRIVE_FILE_ERROR_NOT_FOUND;

    case google_apis::DRIVE_UPLOAD_ERROR_NO_SPACE:
      return DRIVE_FILE_ERROR_NO_SPACE;

    case google_apis::DRIVE_UPLOAD_ERROR_CONFLICT:
      return DRIVE_FILE_ERROR_THROTTLED;

    case google_apis::DRIVE_UPLOAD_ERROR_ABORT:
      return DRIVE_FILE_ERROR_ABORT;
  }

  NOTREACHED();
  return DRIVE_FILE_ERROR_FAILED;
}

}  // namespace drive
