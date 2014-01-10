// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_PLATFORM_CHANNEL_H_
#define MOJO_SYSTEM_PLATFORM_CHANNEL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/launch.h"
#include "mojo/system/platform_channel_handle.h"
#include "mojo/system/system_impl_export.h"

class CommandLine;

namespace mojo {
namespace system {

class MOJO_SYSTEM_IMPL_EXPORT PlatformChannel {
 public:
  virtual ~PlatformChannel();

  // Creates a channel if you already have the underlying handle for it, taking
  // ownership of |handle|.
  static scoped_ptr<PlatformChannel> CreateFromHandle(
      const PlatformChannelHandle& handle);

  // Returns the channel's handle, passing ownership.
  PlatformChannelHandle PassHandle();

  bool is_valid() const { return handle_.is_valid(); }

 protected:
  PlatformChannel();

  PlatformChannelHandle* mutable_handle() { return &handle_; }

 private:
  PlatformChannelHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(PlatformChannel);
};

// This is used to create a pair of connected |PlatformChannel|s. The resulting
// channels can then be used in the same process (e.g., in tests) or between
// processes. (The "server" channel is the one that will be used in the process
// that created the pair, whereas the "client" channel is the one that will be
// used in a different process.)
//
// This class provides facilities for passing the client channel to a child
// process. The parent should call |PrepareToPassClientChannelToChildProcess()|
// to get the data needed to do this, spawn the child using that data, and then
// call |ChildProcessLaunched()|. Note that on Windows this facility (will) only
// work on Vista and later (TODO(vtl)).
//
// Note: |PlatformChannelPair()|, |CreateClientChannelFromParentProcess()|,
// |PrepareToPassClientChannelToChildProcess()|, and |ChildProcessLaunched()|
// have platform-specific implementations.
class MOJO_SYSTEM_IMPL_EXPORT PlatformChannelPair {
 public:
  PlatformChannelPair();
  ~PlatformChannelPair();

  // This transfers ownership of the server channel to the caller. Returns null
  // on failure.
  scoped_ptr<PlatformChannel> CreateServerChannel();

  // For in-process use (e.g., in tests). This transfers ownership of the client
  // channel to the caller. Returns null on failure.
  scoped_ptr<PlatformChannel> CreateClientChannel();

  // To be called in the child process, after the parent process called
  // |PrepareToPassClientChannelToChildProcess()| and launched the child (using
  // the provided data), to create a client channel connected to the server
  // channel (in the parent process). Returns null on failure.
  static scoped_ptr<PlatformChannel> CreateClientChannelFromParentProcess(
      const CommandLine& command_line);

  // Prepares to pass the client channel to a new child process, to be launched
  // using |LaunchProcess()| (from base/launch.h). Modifies |*command_line| and
  // |*file_handle_mapping| as needed. (|file_handle_mapping| may be null on
  // platforms that don't need it, like Windows.)
  void PrepareToPassClientChannelToChildProcess(
      CommandLine* command_line,
      base::FileHandleMappingVector* file_handle_mapping) const;
  // To be called once the child process has been successfully launched, to do
  // any cleanup necessary.
  void ChildProcessLaunched();

 private:
  PlatformChannelHandle server_handle_;
  PlatformChannelHandle client_handle_;

  DISALLOW_COPY_AND_ASSIGN(PlatformChannelPair);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_PLATFORM_CHANNEL_H_
