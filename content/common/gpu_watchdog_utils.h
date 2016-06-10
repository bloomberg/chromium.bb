// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_WATCHDOG_UTILS_H_
#define CONTENT_COMMON_GPU_WATCHDOG_UTILS_H_

#include "content/common/content_export.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {

// Gets temporary file used by GPU watchdog thread.
// Returns false if temporary directory path can't be obtained.
CONTENT_EXPORT bool GetGpuWatchdogTempFile(base::FilePath* file_path);

}  // namespace content

#endif  // CONTENT_COMMON_GPU_WATCHDOG_UTILS_H_
