// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_UTIL_H_
#define BASE_DEBUG_UTIL_H_
#pragma once

#include "build/build_config.h"

class DebugUtil {
 public:
#if defined(OS_MACOSX)
  // On Mac OS X, it can take a really long time for the OS crash handler to
  // process a Chrome crash when debugging symbols are available.  This
  // translates into a long wait until the process actually dies.  This call
  // disables Apple Crash Reporter entirely.
  static void DisableOSCrashDumps();
#endif  // defined(OS_MACOSX)

  // This should be used only in test code.
  static void SuppressDialogs() {
    suppress_dialogs_ = true;
  }

  static bool AreDialogsSuppressed() {
    return suppress_dialogs_;
  }

 private:
  // If true, avoid displaying any dialogs that could cause problems
  // in non-interactive environments.
  static bool suppress_dialogs_;
};

#endif  // BASE_DEBUG_UTIL_H_
