// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_BREAKPAD_WIN_H_
#define CHROME_APP_BREAKPAD_WIN_H_
#pragma once

#include <windows.h>
#include <string>

// The maximum number of 64-char URL chunks we will report.
static const int kMaxUrlChunks = 8;

// Calls InitCrashReporterThread in it's own thread for the browser process
// or directly for the plugin and renderer process.
void InitCrashReporterWithDllPath(const std::wstring& dll_path);

// Intercepts a crash but does not process it, just ask if we want to restart
// the browser or not.
void InitDefaultCrashCallback(LPTOP_LEVEL_EXCEPTION_FILTER filter);

// If chrome has been restarted because it crashed, this function will display
// a dialog asking for permission to continue execution or to exit now.
bool ShowRestartDialogIfCrashed(bool* exit_now);

#endif  // CHROME_APP_BREAKPAD_WIN_H_
