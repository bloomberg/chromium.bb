// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/watcher_client_win.h"

#include <windows.h>

#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/stringprintf.h"
#include "base/win/windows_version.h"
#include "components/browser_watcher/exit_code_watcher_win.h"

namespace browser_watcher {

namespace {

base::Process OpenOwnProcessInheritable() {
  return base::Process(
      ::OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION,
                    TRUE,  // Ineritable handle.
                    base::GetCurrentProcId()));
}

void AddHandleArgument(base::ProcessHandle handle,
                       base::CommandLine* cmd_line) {
  cmd_line->AppendSwitchASCII(ExitCodeWatcher::kParenthHandleSwitch,
                              base::StringPrintf("%d", handle));
}

}  // namespace

WatcherClient::WatcherClient(const base::CommandLine& base_command_line) :
    use_legacy_launch_(base::win::GetVersion() < base::win::VERSION_VISTA),
    base_command_line_(base_command_line) {
}

void WatcherClient::LaunchWatcherProcess(const base::CommandLine& cmd_line,
                                         base::Process self) {
  base::HandlesToInheritVector to_inherit;
  base::LaunchOptions options;
  options.start_hidden = true;
  if (use_legacy_launch_) {
    // Launch the child process inheriting all handles on XP.
    options.inherit_handles = true;
  } else {
    // Launch the child process inheriting only |handle| on
    // Vista and better.
    to_inherit.push_back(self.Handle());
    options.handles_to_inherit = &to_inherit;
  }

  process_ = base::LaunchProcess(cmd_line, options);
  if (!process_.IsValid())
    LOG(ERROR) << "Failed to launch browser watcher.";
}

void WatcherClient::LaunchWatcher() {
  DCHECK(!process_.IsValid());

  // Build the command line for the watcher process.
  base::Process self = OpenOwnProcessInheritable();
  DCHECK(self.IsValid());
  base::CommandLine cmd_line(base_command_line_);
  AddHandleArgument(self.Handle(), &cmd_line);

  // Launch it.
  LaunchWatcherProcess(cmd_line, self.Pass());
}

}  // namespace browser_watcher
