// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_BROKER_MESSAGES_H_
#define MOJO_EDK_SYSTEM_BROKER_MESSAGES_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/process/process_handle.h"

namespace mojo {
namespace edk {

// Pack all structs here.
#pragma pack(push, 1)

// This header defines the message format between ChildBroker and
// ChildBrokerHost.

// Sandbox processes need the parent's help to create shared buffers.
// They are sent synchronously from child to parent and each have
// a response. They are sent over a raw pipe.
enum SandboxMessages : uint32_t {
#if defined(OS_WIN)
  // The reply is two HANDLEs.
  CREATE_PLATFORM_CHANNEL_PAIR = 0,
  // The reply is tokens of the same count of passed in handles.
  HANDLE_TO_TOKEN,
  // The reply is handles of the same count of passed in tokens.
  TOKEN_TO_HANDLE,
#else
  // The reply is a PlatformHandle.
  CREATE_SHARED_BUFFER = 0,
#endif
};

// Definitions of the raw bytes sent in messages.

struct BrokerMessage {
  uint32_t size;
  SandboxMessages id;

#if defined(OS_WIN)
  // Data, if any, follows.
  union {
    HANDLE handles[1];  // If HANDLE_TO_TOKEN.
    uint64_t tokens[1];  // If TOKEN_TO_HANDLE.
  };
#else
  uint32_t shared_buffer_size;  // Size of the shared buffer to create.
#endif
};

const int kBrokerMessageHeaderSize = sizeof(uint32_t) + sizeof(SandboxMessages);

// Route id used for messages between ChildBroker and ChildBrokerHost.
const uint64_t kBrokerRouteId = 1;

// Multiplexing related messages. They are all asynchronous messages.
// They are sent over RawChannel.
enum MultiplexMessages : uint32_t {
  // Messages from child to parent.

  // Tells the parent that the given pipe id has been bound to a
  // MessagePipeDispatcher in the child process. The parent will then respond
  // with either PEER_PIPE_CONNECTED or PEER_DIED when the other side is also
  // bound.
  CONNECT_MESSAGE_PIPE = 0,
  // Tells the parent to remove its bookkeeping for the given peer id since
  // another MessagePipeDispatcher has connected to the pipe in the same
  // process.
  CANCEL_CONNECT_MESSAGE_PIPE,


  // Messages from parent to child.

  // Tells the child to open a channel to a given process. This will be followed
  // by a PEER_PIPE_CONNECTED connecting a message pipe from the child process
  // to the given process over the new channel.
  CONNECT_TO_PROCESS,

  // Connect a given message pipe to another process.
  PEER_PIPE_CONNECTED,

  // Informs the child that the other end of the message pipe is in a process
  // that died.
  PEER_DIED,
};

struct ConnectMessagePipeMessage {
  // CONNECT_MESSAGE_PIPE or CANCEL_CONNECT_MESSAGE_PIPE
  MultiplexMessages type;
  uint64_t pipe_id;
};

struct ConnectToProcessMessage {
  MultiplexMessages type;  // CONNECT_TO_PROCESS
  base::ProcessId process_id;
  // Also has an attached platform handle.
};

struct PeerPipeConnectedMessage {
  MultiplexMessages type;  // PEER_PIPE_CONNECTED
  uint64_t pipe_id;
  base::ProcessId process_id;
};

struct PeerDiedMessage {
  MultiplexMessages type;  // PEER_DIED
  uint64_t pipe_id;
};

#pragma pack(pop)

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_BROKER_MESSAGES_H_
