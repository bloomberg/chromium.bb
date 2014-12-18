// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_BROWSER_WATCHER_EXIT_CODE_WATCHER_WIN_H_
#define COMPONENTS_BROWSER_WATCHER_EXIT_CODE_WATCHER_WIN_H_

#include "base/macros.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"

namespace base {
class CommandLine;
}

namespace browser_watcher {

// Watches for the exit code of a process and records it in a given registry
// location.
class ExitCodeWatcher {
 public:
  // Name of the switch used for the parent process handle.
  static const char kParenthHandleSwitch[];

  // Initialize the watcher with a registry path.
  explicit ExitCodeWatcher(const base::char16* registry_path);
  ~ExitCodeWatcher();

  // Initializes from arguments on |cmd_line|, returns true on success.
  // This function expects the process handle indicated by kParentHandleSwitch
  // in |cmd_line| to be open with sufficient privilege to wait and retrieve
  // the process exit code.
  // It checks the handle for validity and takes ownership of it.
  // The intent is for this handle to be inherited into the watcher process
  // hosting the instance of this class.
  bool ParseArguments(const base::CommandLine& cmd_line);

  // Waits for the process to exit and records its exit code in registry.
  // This is a blocking call.
  void WaitForExit();

  const base::Process& process() const { return process_; }

 private:
  // Writes |exit_code| to registry, returns true on success.
  bool WriteProcessExitCode(int exit_code);

  // The registry path the exit codes are written to.
  base::string16 registry_path_;

  // Handle, PID, and creation time of the watched process.
  base::Process process_;
  base::Time process_creation_time_;

  DISALLOW_COPY_AND_ASSIGN(ExitCodeWatcher);
};

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_EXIT_CODE_WATCHER_WIN_H_
