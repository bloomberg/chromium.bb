// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_BREAKPAD_LINUX_H_
#define CHROME_APP_BREAKPAD_LINUX_H_
#pragma once

#include "base/basictypes.h"

extern void InitCrashReporter();
bool IsCrashReporterEnabled();

static const size_t kMaxActiveURLSize = 1024;
static const size_t kGuidSize = 32;  // 128 bits = 32 chars in hex.
static const size_t kDistroSize = 128;

struct BreakpadInfo {
  const char* filename;            // Path to the Breakpad dump data.
  const char* process_type;        // Process type, e.g. "renderer".
  unsigned process_type_length;    // Length of |process_type|.
  const char* crash_url;           // Active URL in the crashing process.
  unsigned crash_url_length;       // Length of |crash_url|.
  const char* guid;                // Client ID.
  unsigned guid_length;            // Length of |guid|.
  const char* distro;              // Linux distro string.
  unsigned distro_length;          // Length of |distro|.
  bool upload;                     // Whether to upload or save crash dump.
  uint64_t process_start_time;     // Uptime of the crashing process.
  size_t oom_size;                 // Amount of memory requested if OOM.
  uint64_t pid;                    // PID where applicable.
};

extern void HandleCrashDump(const BreakpadInfo& info);

#endif  // CHROME_APP_BREAKPAD_LINUX_H_
