// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_APP_BREAKPAD_WIN_H_
#define COMPONENTS_CRASH_APP_BREAKPAD_WIN_H_

#include <windows.h>
#include <string>
#include <vector>

namespace breakpad {

void InitCrashReporter(const std::string& process_type_switch);

// If chrome has been restarted because it crashed, this function will display
// a dialog asking for permission to continue execution or to exit now.
bool ShowRestartDialogIfCrashed(bool* exit_now);

}  // namespace breakpad

#endif  // COMPONENTS_CRASH_APP_BREAKPAD_WIN_H_
