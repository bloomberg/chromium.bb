// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/fileapi/drivefs_async_file_util.h"

#include "storage/browser/fileapi/local_file_util.h"

namespace drive {
namespace internal {

DriveFsAsyncFileUtil::DriveFsAsyncFileUtil()
    : AsyncFileUtilAdapter(new storage::LocalFileUtil) {}

DriveFsAsyncFileUtil::~DriveFsAsyncFileUtil() = default;

}  // namespace internal
}  // namespace drive
