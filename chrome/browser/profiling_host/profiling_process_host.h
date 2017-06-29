// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_
#define CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_

#include "base/macros.h"
#include "base/process/process.h"

namespace base {
class CommandLine;
}

namespace profiling {

// Represents the browser side of the profiling process (//chrome/profiling).
//
// The profiling process is a singleton. The profiling process infrastructure
// is implemented in the Chrome layer so doesn't depend on the content
// infrastructure for managing child processes.
//
// Not thread safe. Should be used on the browser UI thread only.
//
// The profing process host can be started normally while Chrome is running,
// but can also start in a partial mode where the memory logging connections
// are active but the Mojo control channel has not yet been connected. This is
// to support starting the profiling process very early in the startup
// process (to get most memory events) before other infrastructure like the
// I/O thread has been started.
class ProfilingProcessHost {
 public:
  // Launches the profiling process if necessary and returns a pointer to it.
  static ProfilingProcessHost* EnsureStarted();

  // Returns a pointer to the current global profiling process host or, if
  // no profiling process is launched, nullptr.
  static ProfilingProcessHost* Get();

  // Appends necessary switches to a command line for a child process so it can
  // be profiled. These switches will cause the child process to start in the
  // same mode (either profiling or not) as the browser process.
  static void AddSwitchesToChildCmdLine(base::CommandLine* child_cmd_line);

 private:
  ProfilingProcessHost();
  ~ProfilingProcessHost();

  void Launch();

  // Use process_.IsValid() to determine if the child process has been launched.
  base::Process process_;
  std::string pipe_id_;

  DISALLOW_COPY_AND_ASSIGN(ProfilingProcessHost);
};

}  // namespace profiling

#endif  // CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_
