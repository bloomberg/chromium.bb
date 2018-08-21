// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILEAPI_DRIVEFS_ASYNC_FILE_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILEAPI_DRIVEFS_ASYNC_FILE_UTIL_H_

#include "base/macros.h"
#include "storage/browser/fileapi/async_file_util_adapter.h"

namespace drive {
namespace internal {

// The implementation of storage::AsyncFileUtil for DriveFS File System. This
// forwards to a AsyncFileUtil for native files by default.
class DriveFsAsyncFileUtil : public storage::AsyncFileUtilAdapter {
 public:
  DriveFsAsyncFileUtil();
  ~DriveFsAsyncFileUtil() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveFsAsyncFileUtil);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILEAPI_DRIVEFS_ASYNC_FILE_UTIL_H_
