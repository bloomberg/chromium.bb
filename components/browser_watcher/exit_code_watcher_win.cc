// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/exit_code_watcher_win.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/kill.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/win/registry.h"

namespace browser_watcher {

namespace {

base::string16 GetValueName(const base::Time creation_time,
                            base::ProcessId pid) {
  // Convert the PID and creation time to a string value unique to this
  // process instance.
  return base::StringPrintf(L"%d-%lld", pid, creation_time.ToInternalValue());
}

}  // namespace

const char ExitCodeWatcher::kParenthHandleSwitch[] = "parent-handle";

ExitCodeWatcher::ExitCodeWatcher(const base::char16* registry_path) :
    registry_path_(registry_path) {
}

ExitCodeWatcher::~ExitCodeWatcher() {
}

bool ExitCodeWatcher::ParseArguments(const base::CommandLine& cmd_line) {
  std::string process_handle_str =
      cmd_line.GetSwitchValueASCII(kParenthHandleSwitch);
  unsigned process_handle_uint = 0;
  if (process_handle_str.empty() ||
      !base::StringToUint(process_handle_str, &process_handle_uint)) {
    LOG(ERROR) << "Missing or invalid parent-handle argument.";
    return false;
  }

  HANDLE process_handle = reinterpret_cast<HANDLE>(process_handle_uint);
  // Initial test of the handle, a zero PID indicates invalid handle, or not
  // a process handle. In this case, bail immediately and avoid closing the
  // handle.
  DWORD process_pid = ::GetProcessId(process_handle);
  if (process_pid == 0) {
    LOG(ERROR) << "Invalid parent-handle, can't get parent PID.";
    return false;
  }

  FILETIME creation_time = {};
  FILETIME dummy = {};
  if (!::GetProcessTimes(process_handle, &creation_time,
                         &dummy, &dummy, &dummy)) {
    LOG(ERROR) << "Invalid parent handle, can't get parent process times.";
    return false;
  }

  // Success, take ownership of the process handle.
  process_ = base::Process(process_handle);
  process_creation_time_ = base::Time::FromFileTime(creation_time);

  // Start by writing the value STILL_ACTIVE to registry, to allow detection
  // of the case where the watcher itself is somehow terminated before it can
  // write the process' actual exit code.
  return WriteProcessExitCode(STILL_ACTIVE);
}

void ExitCodeWatcher::WaitForExit() {
  int exit_code = 0;
  if (!process_.WaitForExit(&exit_code)) {
    LOG(ERROR) << "Failed to wait for process.";
    return;
  }

  WriteProcessExitCode(exit_code);
}

bool ExitCodeWatcher::WriteProcessExitCode(int exit_code) {
  base::win::RegKey key(HKEY_CURRENT_USER,
                        registry_path_.c_str(),
                        KEY_WRITE);
  base::string16 value_name(
      GetValueName(process_creation_time_, process_.pid()));

  ULONG result = key.WriteValue(value_name.c_str(), exit_code);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to write to registry, error " << result;
    return false;
  }

  return true;
}

}  // namespace browser_watcher
