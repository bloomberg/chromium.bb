// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_EMBEDDER_PLATFORM_CHANNEL_PAIR_H_
#define MOJO_SYSTEM_EMBEDDER_PLATFORM_CHANNEL_PAIR_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/launch.h"
#include "mojo/system/embedder/scoped_platform_handle.h"
#include "mojo/system/system_impl_export.h"

class CommandLine;

namespace mojo {
namespace embedder {

// This is used to create a pair of |PlatformHandle|s that are connected by a
// suitable (platform-specific) bidirectional "pipe" (e.g., socket on POSIX,
// named pipe on Windows). The resulting handles can then be used in the same
// process (e.g., in tests) or between processes. (The "server" handle is the
// one that will be used in the process that created the pair, whereas the
// "client" handle is the one that will be used in a different process.)
//
// This class provides facilities for passing the client handle to a child
// process. The parent should call |PrepareToPassClientHandlelToChildProcess()|
// to get the data needed to do this, spawn the child using that data, and then
// call |ChildProcessLaunched()|. Note that on Windows this facility (will) only
// work on Vista and later (TODO(vtl)).
//
// Note: |PlatformChannelPair()|, |PassClientHandleFromParentProcess()|,
// |PrepareToPassClientHandleToChildProcess()|, and |ChildProcessLaunched()|
// have platform-specific implementations.
class MOJO_SYSTEM_IMPL_EXPORT PlatformChannelPair {
 public:
  PlatformChannelPair();
  ~PlatformChannelPair();

  ScopedPlatformHandle PassServerHandle();

  // For in-process use (e.g., in tests).
  ScopedPlatformHandle PassClientHandle();

  // To be called in the child process, after the parent process called
  // |PrepareToPassClientHandleToChildProcess()| and launched the child (using
  // the provided data), to create a client handle connected to the server
  // handle (in the parent process).
  static ScopedPlatformHandle PassClientHandleFromParentProcess(
      const CommandLine& command_line);

  // Prepares to pass the client channel to a new child process, to be launched
  // using |LaunchProcess()| (from base/launch.h). Modifies |*command_line| and
  // |*file_handle_mapping| as needed. (|file_handle_mapping| may be null on
  // platforms that don't need it, like Windows.)
  void PrepareToPassClientHandleToChildProcess(
      CommandLine* command_line,
      base::FileHandleMappingVector* file_handle_mapping) const;
  // To be called once the child process has been successfully launched, to do
  // any cleanup necessary.
  void ChildProcessLaunched();

 private:
  ScopedPlatformHandle server_handle_;
  ScopedPlatformHandle client_handle_;

  DISALLOW_COPY_AND_ASSIGN(PlatformChannelPair);
};

}  // namespace embedder
}  // namespace mojo

#endif  // MOJO_SYSTEM_EMBEDDER_PLATFORM_CHANNEL_PAIR_H_
