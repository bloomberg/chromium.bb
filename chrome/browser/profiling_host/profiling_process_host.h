// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_
#define CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_

#include "base/macros.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/profiling/profiling_control.mojom.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"

// The .mojom include above may not be generated unless OOP heap profiling is
// enabled.
#if !BUILDFLAG(ENABLE_OOP_HEAP_PROFILING)
#error profiling_process_host.h should only be included with OOP heap profiling
#endif

namespace base {
class CommandLine;
}  // namespace base

#if defined(OS_POSIX) && !defined(OS_MACOSX)
namespace content {
class FileDescriptorInfo;
}  // namespace content
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

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

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  static void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::FileDescriptorInfo* mappings);
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

  // Sends a message to the profiling process that it dump the given process'
  // memory data.
  void RequestProcessDump(base::ProcessId pid);

 private:
  ProfilingProcessHost();
  ~ProfilingProcessHost();

  void Launch();

  void EnsureControlChannelExists();
  void ConnectControlChannelOnIO();
  void AddNewSenderOnIO(mojo::edk::ScopedPlatformHandle handle,
                        int child_process_id);

  // Use process_.IsValid() to determine if the child process has been launched.
  base::Process process_;
  std::string pipe_id_;

  // IO thread only -----------------------------------------------------------
  //
  // Once the constructor is finished, the following variables must only be
  // accessed on the IO thread.

  // Holds the pending server handle for the Mojo control channel during
  // the period between the profiling process launching and the Mojo channel
  // being created. Will be invalid otherwise.
  mojo::edk::ScopedPlatformHandle pending_control_connection_;

  mojom::ProfilingControlPtr profiling_control_;

  DISALLOW_COPY_AND_ASSIGN(ProfilingProcessHost);
};

}  // namespace profiling

#endif  // CHROME_BROWSER_PROFILING_HOST_PROFILING_PROCESS_HOST_H_
