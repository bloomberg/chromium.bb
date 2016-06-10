// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu_watchdog_utils.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"

namespace content {

bool GetGpuWatchdogTempFile(base::FilePath* file_path) {
  base::FilePath temp_dir_path;
  if (GetTempDir(&temp_dir_path)) {
    *file_path = temp_dir_path.Append(FILE_PATH_LITERAL("gpu-watchdog.tmp"));
    return true;
  }

  return false;
}

}  // namespace content
