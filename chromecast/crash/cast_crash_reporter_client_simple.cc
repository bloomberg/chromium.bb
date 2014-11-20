// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/cast_crash_reporter_client.h"

#include "base/logging.h"

namespace chromecast {

bool CastCrashReporterClient::HandleCrashDump(const char* crashdump_filename) {
  LOG(INFO) << "Process " << GetProcessType() << " crashed; minidump in "
            << crashdump_filename;
  return true;
}

}  // namespace chromecast
