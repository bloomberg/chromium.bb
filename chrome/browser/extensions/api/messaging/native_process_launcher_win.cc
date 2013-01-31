// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"

#include <windows.h>

#include "base/logging.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"

namespace extensions {

// static
bool NativeProcessLauncher::LaunchNativeProcess(
    const FilePath& path,
    base::ProcessHandle* native_process_handle,
    base::PlatformFile* read_file,
    base::PlatformFile* write_file) {
  NOTREACHED();
  return false;
}

}  // namespace extensions
