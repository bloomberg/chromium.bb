// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HANG_MONITOR_HANG_CRASH_DUMP_WIN_H_
#define CHROME_BROWSER_HANG_MONITOR_HANG_CRASH_DUMP_WIN_H_

#include <windows.h>

#include "base/strings/string_split.h"

// Causes the given child process to generate a crash dump and terminates the
// process. |additional_serialized_crash_keys| are additional key/value string
// pairs that will be logged in the child crash report. The crash keys provided
// must be preregistered before calling this method.
void CrashDumpAndTerminateHungChildProcess(
    HANDLE hprocess,
    const base::StringPairs& additional_crash_keys);

// TODO(yzshen): Remove when enough information is collected and the hang rate
// of pepper/renderer processes is reduced.
void CrashDumpForHangDebugging(HANDLE hprocess);

#endif  // CHROME_BROWSER_HANG_MONITOR_HANG_CRASH_DUMP_WIN_H_
