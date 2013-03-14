// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_FRAME_REPORTING_H_
#define CHROME_FRAME_CHROME_FRAME_REPORTING_H_

#include "chrome_frame/scoped_initialization_manager.h"

namespace chrome_frame {

// A Traits class for a ScopedInitializationManager that starts/stops crash
// reporting for npchrome_frame.dll.
class CrashReportingTraits {
 public:
  static void Initialize();
  static void Shutdown();
};

// Manages crash reporting for the Chrome Frame dll. Crash reporting cannot be
// reliably started or stopped when the loader lock is held, so DllMain cannot
// be used to start/stop reporting. Rather, instances of this class are used in
// each entrypoint into the dll.
typedef ScopedInitializationManager<CrashReportingTraits> ScopedCrashReporting;

}  // namespace chrome_frame

#endif  // CHROME_FRAME_CHROME_FRAME_REPORTING_H_
