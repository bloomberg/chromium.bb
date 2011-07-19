// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILING_H_
#define CHROME_COMMON_PROFILING_H_
#pragma once

#include "build/build_config.h"

#include "base/basictypes.h"
#include "base/debug/profiler.h"

// The Profiling class manages the interaction with a sampling based profiler.
// Its function is controlled by the kProfilingAtStart, kProfilingFile, and
// kProfilingFlush command line values.
// All of the API should only be called from the main thread of the process.
class Profiling {
 public:
  // Called early in a process' life to allow profiling of startup time.
  // the presence of kProfilingAtStart is checked.
  static void ProcessStarted();

  // Start profiling.
  static void Start();

  // Stop profiling and write out profiling file.
  static void Stop();

  // Returns true if the process is being profiled.
  static bool BeingProfiled();

  // Called when the main message loop is started, so that automatic flushing
  // of the profile data file can be done.
  static void MainMessageLoopStarted();

  // Toggle profiling on/off.
  static void Toggle();

 private:
  // Do not instantiate this class.
  Profiling();

  DISALLOW_COPY_AND_ASSIGN(Profiling);
};

#endif  // CHROME_COMMON_PROFILING_H_

