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

// This header defines the message format between ChildBroker and
// ChildBrokerHost.

#if defined(OS_WIN)
// Windows only messages needed because sandboxed child processes need the
// parent's help. They are sent synchronously from child to parent and each have
// a response. They are sent over a raw pipe.
enum WindowsSandboxMessages {
  // The reply is two HANDLEs.
  CREATE_PLATFORM_CHANNEL_PAIR = 0,
  // The reply is tokens of the same count of passed in handles.
  HANDLE_TO_TOKEN,
  // The reply is handles of the same count of passed in tokens.
  TOKEN_TO_HANDLE,
};

// Definitions of the raw bytes sent in messages.

struct BrokerMessage {
  uint32_t size;
  WindowsSandboxMessages id;
  // Data, if any, follows.
  union {
    HANDLE handles[1];  // If HANDLE_TO_TOKEN.
    uint64_t tokens[1];  // If TOKEN_TO_HANDLE.
  };
};

const int kBrokerMessageHeaderSize =
  sizeof(uint32_t) + sizeof(WindowsSandboxMessages);

#endif

// Route id used for messages between ChildBroker and ChildBrokerHost.
const uint64_t kBrokerRouteId = 1;

// Multiplexing related messages. They are all asynchronous messages.
// They are sent over RawChannel.
enum MultiplexMessages {
  // Messages from child to parent.
  CONNECT_MESSAGE_PIPE = 0,
  CANCEL_CONNECT_MESSAGE_PIPE,

  // Messages from parent to child.
  CONNECT_TO_PROCESS,
  PEER_PIPE_CONNECTED,
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

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_BROKER_MESSAGES_H_
