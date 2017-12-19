// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CRASH_KEYS_H_
#define CHROME_COMMON_CRASH_KEYS_H_

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/debug/crash_logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_keys.h"

namespace base {
class CommandLine;
}

namespace crash_keys {

// Registers all of the potential crash keys that can be sent to the crash
// reporting server. Returns the size of the union of all keys.
size_t RegisterChromeCrashKeys();

// Sets the kNumSwitches key and the set of keys named using kSwitchFormat based
// on the given |command_line|.
void SetCrashKeysFromCommandLine(const base::CommandLine& command_line);

// Sets the list of "active" extensions in this process. We overload "active" to
// mean different things depending on the process type:
// - browser: all enabled extensions
// - renderer: the unique set of extension ids from all content scripts
// - extension: the id of each extension running in this process (there can be
//   multiple because of process collapsing).
void SetActiveExtensions(const std::set<std::string>& extensions);

// Sets the printer info. Data should be separated by ';' up to
// kPrinterInfoCount substrings. Each substring will be truncated if necessary.
class ScopedPrinterInfo {
 public:
  explicit ScopedPrinterInfo(const base::StringPiece& data);
  ~ScopedPrinterInfo();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedPrinterInfo);
};

// Crash Key Name Constants ////////////////////////////////////////////////////

// The URL actively being processed in some way.
extern const char kActiveURL[];

}  // namespace crash_keys

#endif  // CHROME_COMMON_CRASH_KEYS_H_
