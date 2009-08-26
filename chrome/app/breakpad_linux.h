// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_BREAKPAD_LINUX_H_
#define CHROME_APP_BREAKPAD_LINUX_H_

#include <string>

extern void InitCrashReporter();

#if defined(USE_LINUX_BREAKPAD)
static const size_t kMaxActiveURLSize = 1024;
static const size_t kGuidSize = 32;  // 128 bits = 32 chars in hex.
static const size_t kDistroSize = 128;

struct BreakpadInfo {
  const char* filename;
  const char* process_type;
  unsigned process_type_length;
  const char* crash_url;
  unsigned crash_url_length;
  const char* guid;
  unsigned guid_length;
  const char* distro;
  unsigned distro_length;
  bool upload;
};

extern int HandleCrashDump(const BreakpadInfo& info);
#endif  // defined(USE_LINUX_BREAKPAD)

#if defined(GOOGLE_CHROME_BUILD)
// Checks that the kernel's core filename pattern is "core" and moves the
// current working directory to a temp directory.
// Returns true iff core dumping has been successfully enabled for the current
// process.
bool EnableCoreDumping(std::string* core_dump_directory);
// Blocks until the given child has exited. If the kernel indicates that the
// child dumped core, then the core is expected a file called "core" and is
// uploaded to a collection server. The core file is deleted and the given
// directory is removed.
void MonitorForCoreDumpsAndReport(const std::string& core_dump_directory,
                                  const pid_t child);

#endif  // defined(GOOGLE_CHROME_BUILD)

#endif  // CHROME_APP_BREAKPAD_LINUX_H_
