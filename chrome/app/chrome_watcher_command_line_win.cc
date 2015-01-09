// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_watcher_command_line_win.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/chrome_switches.h"

namespace {
const char kParentHandleSwitch[] = "parent-handle";
}  // namespace

base::CommandLine GenerateChromeWatcherCommandLine(
    const base::FilePath& chrome_exe,
    HANDLE parent_process) {
  base::CommandLine command_line(chrome_exe);
  command_line.AppendSwitchASCII(switches::kProcessType, "watcher");
  command_line.AppendSwitchASCII(
      kParentHandleSwitch,
      base::UintToString(reinterpret_cast<unsigned int>(parent_process)));
  return command_line;
}

base::win::ScopedHandle InterpretChromeWatcherCommandLine(
    const base::CommandLine& command_line) {
  std::string parent_handle_str =
      command_line.GetSwitchValueASCII(kParentHandleSwitch);
  unsigned parent_handle_uint = 0;
  if (parent_handle_str.empty() ||
      !base::StringToUint(parent_handle_str, &parent_handle_uint)) {
    LOG(ERROR) << "Missing or invalid " << kParentHandleSwitch << " argument.";
    return base::win::ScopedHandle();
  }

  HANDLE parent_process = reinterpret_cast<HANDLE>(parent_handle_uint);
  // Initial test of the handle, a zero PID indicates invalid handle, or not
  // a process handle. In this case, parsing fails and we avoid closing the
  // handle.
  DWORD process_pid = ::GetProcessId(parent_process);
  if (process_pid == 0) {
    LOG(ERROR) << "Invalid " << kParentHandleSwitch
               << " argument. Can't get parent PID.";
    return base::win::ScopedHandle();
  }

  return base::win::ScopedHandle(parent_process);
}
