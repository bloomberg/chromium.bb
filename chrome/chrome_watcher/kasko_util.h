// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_WATCHER_KASKO_UTIL_H_
#define CHROME_CHROME_WATCHER_KASKO_UTIL_H_

#include <windows.h>

#include "base/process/process.h"
#include "base/strings/string16.h"

// Initialize a Kasko reporter.
bool InitializeKaskoReporter(const base::string16& endpoint,
                             const base::char16* browser_data_directory);

// Shut down a Kasko reporter.
void ShutdownKaskoReporter();

// Ensure that a process is a valid target for report capture. This is to
// defend against against races that exist in getting a Process from a window
// name (potential HWND and process id recycling).
bool EnsureTargetProcessValidForCapture(const base::Process& process);

// Dumps a process. A crash key will be added to the report, labelling it as a
// hang report.
void DumpHungProcess(DWORD main_thread_id, const base::string16& channel,
                     const base::char16* hang_type,
                     const base::Process& process);

#endif  // CHROME_CHROME_WATCHER_KASKO_UTIL_H_
