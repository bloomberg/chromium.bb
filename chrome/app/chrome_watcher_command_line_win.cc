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

const char kOnIninitializedEventHandleSwitch[] = "on-initialized-event-handle";
const char kParentHandleSwitch[] = "parent-handle";

void AppendHandleSwitch(const std::string& switch_name,
                        HANDLE handle,
                        base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(
      switch_name, base::UintToString(reinterpret_cast<unsigned int>(handle)));
}

HANDLE ReadHandleFromSwitch(const base::CommandLine& command_line,
                            const std::string& switch_name) {
  std::string switch_string = command_line.GetSwitchValueASCII(switch_name);
  unsigned int switch_uint = 0;
  if (switch_string.empty() ||
      !base::StringToUint(switch_string, &switch_uint)) {
    DLOG(ERROR) << "Missing or invalid " << switch_name << " argument.";
    return nullptr;
  }
  return reinterpret_cast<HANDLE>(switch_uint);
}

}  // namespace

base::CommandLine GenerateChromeWatcherCommandLine(
    const base::FilePath& chrome_exe,
    HANDLE parent_process,
    HANDLE on_initialized_event) {
  base::CommandLine command_line(chrome_exe);
  command_line.AppendSwitchASCII(switches::kProcessType, "watcher");
  AppendHandleSwitch(kOnIninitializedEventHandleSwitch, on_initialized_event,
                     &command_line);
  AppendHandleSwitch(kParentHandleSwitch, parent_process, &command_line);
  return command_line;
}

bool InterpretChromeWatcherCommandLine(
    const base::CommandLine& command_line,
    base::win::ScopedHandle* parent_process,
    base::win::ScopedHandle* on_initialized_event) {
  DCHECK(on_initialized_event);
  DCHECK(parent_process);

  // For consistency, always close any existing HANDLEs here.
  on_initialized_event->Close();
  parent_process->Close();

  HANDLE parent_handle =
      ReadHandleFromSwitch(command_line, kParentHandleSwitch);
  HANDLE on_initialized_event_handle =
      ReadHandleFromSwitch(command_line, kOnIninitializedEventHandleSwitch);

  if (parent_handle) {
    // Initial test of the handle, a zero PID indicates invalid handle, or not
    // a process handle. In this case, parsing fails and we avoid closing the
    // handle.
    DWORD process_pid = ::GetProcessId(parent_handle);
    if (process_pid == 0) {
      DLOG(ERROR) << "Invalid " << kParentHandleSwitch
                  << " argument. Can't get parent PID.";
    } else {
      parent_process->Set(parent_handle);
    }
  }

  if (on_initialized_event_handle) {
    DWORD result = ::WaitForSingleObject(on_initialized_event_handle, 0);
    if (result == WAIT_FAILED) {
      DPLOG(ERROR)
          << "Unexpected error while testing the initialization event.";
    } else if (result != WAIT_TIMEOUT) {
      DLOG(ERROR) << "Unexpected result while testing the initialization event "
                     "with WaitForSingleObject: " << result;
    } else {
      on_initialized_event->Set(on_initialized_event_handle);
    }
  }

  if (!on_initialized_event->IsValid() || !parent_process->IsValid()) {
    // If one was valid and not the other, free the valid one.
    on_initialized_event->Close();
    parent_process->Close();
    return false;
  }

  return true;
}
