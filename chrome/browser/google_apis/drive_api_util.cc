// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_api_util.h"

#include "base/command_line.h"
#include "chrome/browser/google_apis/drive_switches.h"

namespace google_apis {
namespace util {

bool IsDriveV2ApiEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDriveV2Api);
}

}  // namespace util
}  // namespace google_apis
