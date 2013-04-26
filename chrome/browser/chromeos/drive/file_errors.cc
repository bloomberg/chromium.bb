// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_errors.h"

#include "base/logging.h"

namespace drive {

std::string FileErrorToString(FileError error) {
  switch (error) {
    case FILE_ERROR_OK:
      return "FILE_ERROR_OK";

    case FILE_ERROR_FAILED:
      return "FILE_ERROR_FAILED";

    case FILE_ERROR_IN_USE:
      return "FILE_ERROR_IN_USE";

    case FILE_ERROR_EXISTS:
      return "FILE_ERROR_EXISTS";

    case FILE_ERROR_NOT_FOUND:
      return "FILE_ERROR_NOT_FOUND";

    case FILE_ERROR_ACCESS_DENIED:
      return "FILE_ERROR_ACCESS_DENIED";

    case FILE_ERROR_TOO_MANY_OPENED:
      return "FILE_ERROR_TOO_MANY_OPENED";

    case FILE_ERROR_NO_MEMORY:
      return "FILE_ERROR_NO_MEMORY";

    case FILE_ERROR_NO_SPACE:
      return "FILE_ERROR_NO_SPACE";

    case FILE_ERROR_NOT_A_DIRECTORY:
      return "FILE_ERROR_NOT_A_DIRECTORY";

    case FILE_ERROR_INVALID_OPERATION:
      return "FILE_ERROR_INVALID_OPERATION";

    case FILE_ERROR_SECURITY:
      return "FILE_ERROR_SECURITY";

    case FILE_ERROR_ABORT:
      return "FILE_ERROR_ABORT";

    case FILE_ERROR_NOT_A_FILE:
      return "FILE_ERROR_NOT_A_FILE";

    case FILE_ERROR_NOT_EMPTY:
      return "FILE_ERROR_NOT_EMPTY";

    case FILE_ERROR_INVALID_URL:
      return "FILE_ERROR_INVALID_URL";

    case FILE_ERROR_NO_CONNECTION:
      return "FILE_ERROR_NO_CONNECTION";

    case FILE_ERROR_THROTTLED:
      return "FILE_ERROR_THROTTLED";
  }

  NOTREACHED();
  return "";
}

base::PlatformFileError FileErrorToPlatformError(FileError error) {
  switch (error) {
    case FILE_ERROR_OK:
      return base::PLATFORM_FILE_OK;

    case FILE_ERROR_FAILED:
      return base::PLATFORM_FILE_ERROR_FAILED;

    case FILE_ERROR_IN_USE:
      return base::PLATFORM_FILE_ERROR_IN_USE;

    case FILE_ERROR_EXISTS:
      return base::PLATFORM_FILE_ERROR_EXISTS;

    case FILE_ERROR_NOT_FOUND:
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;

    case FILE_ERROR_ACCESS_DENIED:
      return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;

    case FILE_ERROR_TOO_MANY_OPENED:
      return base::PLATFORM_FILE_ERROR_TOO_MANY_OPENED;

    case FILE_ERROR_NO_MEMORY:
      return base::PLATFORM_FILE_ERROR_NO_MEMORY;

    case FILE_ERROR_NO_SPACE:
      return base::PLATFORM_FILE_ERROR_NO_SPACE;

    case FILE_ERROR_NOT_A_DIRECTORY:
      return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;

    case FILE_ERROR_INVALID_OPERATION:
      return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

    case FILE_ERROR_SECURITY:
      return base::PLATFORM_FILE_ERROR_SECURITY;

    case FILE_ERROR_ABORT:
      return base::PLATFORM_FILE_ERROR_ABORT;

    case FILE_ERROR_NOT_A_FILE:
      return base::PLATFORM_FILE_ERROR_NOT_A_FILE;

    case FILE_ERROR_NOT_EMPTY:
      return base::PLATFORM_FILE_ERROR_NOT_EMPTY;

    case FILE_ERROR_INVALID_URL:
      return base::PLATFORM_FILE_ERROR_INVALID_URL;

    case FILE_ERROR_NO_CONNECTION:
      return base::PLATFORM_FILE_ERROR_FAILED;

    case FILE_ERROR_THROTTLED:
      return base::PLATFORM_FILE_ERROR_FAILED;
  }

  NOTREACHED();
  return base::PLATFORM_FILE_ERROR_FAILED;
}

}  // namespace drive
