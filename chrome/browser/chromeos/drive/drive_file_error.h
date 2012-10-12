// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_ERROR_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_ERROR_H_

#include "base/platform_file.h"

namespace gdata {

enum DriveFileError {
  DRIVE_FILE_OK = 0,
  DRIVE_FILE_ERROR_FAILED = -1,
  DRIVE_FILE_ERROR_IN_USE = -2,
  DRIVE_FILE_ERROR_EXISTS = -3,
  DRIVE_FILE_ERROR_NOT_FOUND = -4,
  DRIVE_FILE_ERROR_ACCESS_DENIED = -5,
  DRIVE_FILE_ERROR_TOO_MANY_OPENED = -6,
  DRIVE_FILE_ERROR_NO_MEMORY = -7,
  DRIVE_FILE_ERROR_NO_SPACE = -8,
  DRIVE_FILE_ERROR_NOT_A_DIRECTORY = -9,
  DRIVE_FILE_ERROR_INVALID_OPERATION = -10,
  DRIVE_FILE_ERROR_SECURITY = -11,
  DRIVE_FILE_ERROR_ABORT = -12,
  DRIVE_FILE_ERROR_NOT_A_FILE = -13,
  DRIVE_FILE_ERROR_NOT_EMPTY = -14,
  DRIVE_FILE_ERROR_INVALID_URL = -15,
  DRIVE_FILE_ERROR_NO_CONNECTION = -16,
  DRIVE_FILE_ERROR_THROTTLED = -17,
};

// Returns a PlatformFileError that corresponds to the DriveFileError provided.
base::PlatformFileError DriveFileErrorToPlatformError(DriveFileError error);

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_ERROR_H_
