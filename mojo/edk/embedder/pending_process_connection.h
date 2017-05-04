// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_PENDING_PROCESS_CONNECTION_H_
#define MOJO_EDK_EMBEDDER_PENDING_PROCESS_CONNECTION_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "mojo/edk/embedder/connection_params.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
namespace edk {

using ProcessErrorCallback = base::Callback<void(const std::string& error)>;

// Represents a potential connection to an external process.
//
// NOTE: This class must only be used while IPC support is active. See
// documentation for |InitIPCSupport()| in embedder.h or ScopedIPCSupport in
// scoped_ipc_support.h.
//
// Precise usage depends on the nature of the relationship between the calling
// process and the remote process, but a typical use might look something like:
//
//     mojo::edk::PendingProcessConnection connection;
//
//     std::string pipe_token;
//     ScopedMessagePipeHandle pipe = connection.CreateMessagePipe(&pipe_token);
//
//     // New pipes to the process are fully functional and can be used right
//     // away, even if the process doesn't exist yet.
//     GoDoSomethingInteresting(std::move(pipe));
//
//     mojo::edk::ScopedPlatformChannelPair channel;
//
//     // Give the pipe token to the child process via command-line.
//     child_command_line.AppendSwitchASCII("yer-pipe", pipe_token);
//
//     // Magic child process launcher which gives one end of the pipe to the
//     // new process.
//     LaunchProcess(child_command_line, channel.PassClientHandle());
//
//     // Some time later...
//     connection.Connect(
//         new_process,
//         mojo::edk::ConnectionParams(
//             mojo::edk::ConnectionParams::ConnectionType::kBrokerIntroduction,
//             mojo::edk::ConnectionParams::TransportProtocol::kAutomatic,
//             channel.PassServerHandle()));
//
// If |connection| is destroyed before Connect() can be called, any pipe
// created by |connection.CreateMessagePipe()| will imminently behave as if its
// peer has been closed.
//
// Otherwise if the remote process in this example eventually executes:
//
//     mojo::edk::PendingProcessConnection connection;
//
//     std::string token = command_line.GetSwitchValueASCII("yer-pipe");
//     ScopedMessagePipeHandle pipe = mojo::edk::CreateChildMessagePipe(token);
//
//     connection.Connect(
//         base::kNullProcessHandle,
//         mojo::edk::ConnectionParams(
//             mojo::edk::ConnectionParams::ConnectionType::kBrokerClient,
//             mojo::edk::ConnectionParams::TransportProtocol::kAutomatic))
//
// it will be connected to this process, and its |pipe| will be connected to
// this process's |pipe|.
//
// If the remote process exits or otherwise closes its client channel handle
// before calling CreateChildMessagePipe for a given message pipe token,
// this process's end of the corresponding message pipe will imminently behave
// as if its peer has been closed.
class MOJO_SYSTEM_IMPL_EXPORT PendingProcessConnection {
 public:
  // Selects the type of connection to establish to the remote process. The
  // value names here correspond to the local process's role in the pending
  // connection.
  enum class Type {
    // Connect the process as a broker client. In this case, the remote process
    // must either be the broker itself or another broker client, and its end
    // of the connection must be constructed as |Type::kBrokerIntroduction|.
    //
    // Processes connected in this way become fully functional broker clients
    // and can thus seamlessly send and receive platform object handles to and
    // from the broker and/or any other broker client in the graph of connected
    // processes.
    kBrokerClient,

    // Connect to the process and introduce it to the broker. This is used to
    // introduce new broker clients into the calling process's process graph and
    // may be used by either the broker itself or by any established broker
    // client who wishes to introduce a new broker client process.
    //
    // The other end of the ConnectionParams' underlying connection primitive
    // (e.g. the other end of the channel pipe) must be used to connect a
    // PendingProcessConnection of type |Type::kBrokerClient| within the
    // remote process.
    kBrokerIntroduction,

    // Connect to the process as a peer. This may be used to interconnect two
    // processes in a way that does not support any kind of mutual brokering,
    // i.e. a one-off interprocess connection.
    //
    // Both ends of the ConnectionParams' underlying connection primitive
    // (e.g. both ends of the channel pipe) must ultimately be connected in
    // either process using a PendingProcessConnection of this type.
    //
    // Such connections do NOT guarantee any kind of support for platform handle
    // transmission over message pipes or introduction to any other processes
    // in either peer's graph of connected processes. Mojo handle transmission
    // from either peer to some other process in the other peer's extended
    // process graph may arbitrarily fail without explanation.
    //
    // This connection type should be used rarely, only when it's impossible to
    // make any guarantees about the relative lifetime of the two processes;
    // when there is no suitable broker process in the system; and/or when it's
    // impossible for at least one of the two processes to acquire a
    // |base::ProcessHandle| to the other.
    kPeer,
  };

  // Constructs a PendingProcessConnection of type |Type::kBrokerIntroduction|.
  // This exists to support legacy code which used PendingProcessConnection
  // before different connection types were supported.
  //
  // DEPRECATED. Please use the explicit signature below.
  //
  // TODO(rockot): Remove this constructor.
  PendingProcessConnection();

  // Constructs a PendingProcessConnection of the given |type|. See the
  // documentation for Type above.
  explicit PendingProcessConnection(Type type);

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
  //
  // DEPRECATED: The version of CreateMessagePipe below should be preferred to
  // this one. There is no compelling reason to randomly generate token strings
  // and it is generally more convenient for embedders to use fixed name values
  // on each end of a connection.
  ScopedMessagePipeHandle CreateMessagePipe(std::string* token);

  // Like above, this creates a message pipe whose other end is obtainable in
  // the remote process. Unlike above, the pipe is associated with an arbitrary
  // |name| given by the caller rather than a randomly generated token string.
  //
  // |name| must be unique within the context of this PendingProcessConnection
  // instance, but it's fine to have multiple PendingProcessConnections use the
  // same |name| to each create a unique pipe. In other words, the value of
  // |name| is always scoped to the PendingProcessConnection instance.
  //
  // The remote process can acquire the other end of the returned pipe by
  // passing the same name to CreateMessagePipe on a corresponding
  // PendingProcessConnection. In general, |name| is some string which is
  // already agreed upon ahead of time, typically hard-coded into both processes
  // on either end of the connection.
  //
  // NOTE: It's important to ensure that on kBrokerIntroduction/kBrokerClient
  // connections using kLegacy protocol, the kBrokerIntroduction side must have
  // already created a named pipe by the time the kBrokerClient side attempts to
  // use the same name; otherwise the behavior is racy. This restriction does
  // not apply to symmetrical kPeer/kPeer connections or to any type of
  // connection using a newer (>= kVersion0) TransportProtocol.
  ScopedMessagePipeHandle CreateMessagePipe(const std::string& name);

  // Connects to the process. This must be called at most once, with the process
  // handle in |process|. If the process handle is unknown (as is often the case
  // in broker client processes), it should be |base::kNullProcessHandle|.
  //
  // |connection_params| contains details specifying the precise nature of the
  // connection, including one half of some underlying OS IPC primitive (e.g.
  // a socket FD) to be used for physical transport. The other half must be
  // provided to a corresponding PendingProcessConnection in the remote process.
  // See the documentation for ConnectionParams in connection_params.h.
  //
  // If this method is not called by the time the PendingProcessConnection is
  // destroyed, it's assumed that the process is unavailable (e.g. process
  // launch failed or the process has otherwise been terminated early), and
  // any associated resources will be cleaned up at that time. Message pipe
  // handles returned by |CreateMessagePipe()| above will appear to have their
  // peers closed at this time as well.
  void Connect(
      base::ProcessHandle process,
      ConnectionParams connection_params,
      const ProcessErrorCallback& error_callback = ProcessErrorCallback());

 private:
  const Type type_;

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
