// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_PENDING_PROCESS_CONNECTION_H_
#define MOJO_EDK_EMBEDDER_PENDING_PROCESS_CONNECTION_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
namespace edk {

using ProcessErrorCallback = base::Callback<void(const std::string& error)>;

// Represents a potential connection to an external process. Use this object
// to make other processes reachable from this one via Mojo IPC. Typical usage
// might look something like:
//
//     PendingProcessConnection connection;
//
//     std::string pipe_token;
//     ScopedMessagePipeHandle pipe = connection.CreateMessagePipe(&pipe_token);
//
//     // New pipes to the process are fully functional and can be used right
//     // away, even if the process doesn't exist yet.
//     GoDoSomethingInteresting(std::move(pipe));
//
//     ScopedPlatformChannelPair channel;
//
//     // Give the pipe token to the child process via command-line.
//     child_command_line.AppendSwitchASCII("yer-pipe", pipe_token);
//
//     // Magic child process launcher which gives one end of the pipe to the
//     // new process.
//     LaunchProcess(child_command_line, channel.PassClientHandle());
//
//     // Some time later...
//     connection.Connect(new_process, channel.PassServerHandle());
//
// If at any point during the above process, |connection| is destroyed before
// Connect() can be called, |pipe| will imminently behave as if its peer has
// been closed.
//
// Otherwise, if the remote process in this example eventually calls:
//
//     mojo::edk::SetParentPipeHandle(std::move(client_channel_handle));
//
//     std::string token = command_line.GetSwitchValueASCII("yer-pipe");
//     ScopedMessagePipeHandle pipe = mojo::edk::CreateChildMessagePipe(token);
//
// it will be connected to this process, and its |pipe| will be connected to
// this process's |pipe|.
//
// If the remote process exits or otherwise closes its client channel handle
// before calling CreateChildMessagePipe for a given message pipe token,
// this process's end of the corresponding message pipe will imminently behave
// as if its peer has been closed.
//
class MOJO_SYSTEM_IMPL_EXPORT PendingProcessConnection {
 public:
  PendingProcessConnection();
  ~PendingProcessConnection();

  // Creates a message pipe associated with a new globally unique string value
  // which will be placed in |*token|.
  //
  // The other end of the new pipe is obtainable in the remote process (or in
  // this process, to facilitate "single-process mode" in some applications) by
  // passing the new |*token| value to mojo::edk::CreateChildMessagePipe. It's
  // the caller's responsibility to communicate the value of |*token| to the
  // remote process by any means available, e.g. a command-line argument on
  // process launch, or some other out-of-band communication channel for an
  // existing process.
  //
  // NOTES: This may be called any number of times to create multiple message
  // pipes to the same remote process. This call ALWAYS succeeds, returning
  // a valid message pipe handle and populating |*token| with a new unique
  // string value.
  ScopedMessagePipeHandle CreateMessagePipe(std::string* token);

  // Connects to the process. This must be called at most once, with the process
  // handle in |process|.
  //
  // |channel| is the platform handle of an OS pipe which can be used to
  // communicate with the connected process. The other end of that pipe must
  // ultimately be passed to mojo::edk::SetParentPipeHandle in the remote
  // process, and getting that end of the pipe into the other process is the
  // embedder's responsibility.
  //
  // If this method is not called by the time the PendingProcessConnection is
  // destroyed, it's assumed that the process is unavailable (e.g. process
  // launch failed or the process has otherwise been terminated early), and
  // any associated resources, such as remote endpoints of messages pipes
  // created by CreateMessagePipe above) will be cleaned up at that time.
  void Connect(
      base::ProcessHandle process,
      ScopedPlatformHandle channel,
      const ProcessErrorCallback& error_callback = ProcessErrorCallback());

 private:
  // A GUID representing a potential new process to be connected to this one.
  const std::string process_token_;

  // Indicates whether this object has been used to create new message pipes.
  bool has_message_pipes_ = false;

  // Indicates whether Connect() has been called yet.
  bool connected_ = false;

  DISALLOW_COPY_AND_ASSIGN(PendingProcessConnection);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_PENDING_PROCESS_CONNECTION_H_
