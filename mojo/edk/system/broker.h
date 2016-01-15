// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_BROKER_H_
#define MOJO_EDK_SYSTEM_BROKER_H_

#include <stddef.h>
#include <stdint.h>

#include "mojo/edk/embedder/scoped_platform_handle.h"

namespace mojo {
namespace edk {
class MessagePipeDispatcher;
class RawChannel;
class PlatformSharedBuffer;

// An interface for communicating to a central "broker" process from each
// process using the EDK. It serves two purposes:
// 1) Windows only: brokering to help child processes as they can't create
//    named pipes or duplicate handles.
// 2) All platforms: support multiplexed messages pipes.

class MOJO_SYSTEM_IMPL_EXPORT Broker {
 public:
  virtual ~Broker() {}

#if defined(OS_WIN)
  // It is safe to call these three methods from any thread.

  // Create a PlatformChannelPair.
  virtual void CreatePlatformChannelPair(ScopedPlatformHandle* server,
                                         ScopedPlatformHandle* client) = 0;

  // Converts the given platform handles to tokens.
  // |tokens| should point to memory that is sizeof(uint64_t) * count;
  virtual void HandleToToken(const PlatformHandle* platform_handles,
                             size_t count,
                             uint64_t* tokens) = 0;

  // Converts the given tokens to platformhandles.
  // |handles| should point to memory that is sizeof(HANDLE) * count;
  virtual void TokenToHandle(const uint64_t* tokens,
                             size_t count,
                             PlatformHandle* handles) = 0;
#else
  // Creates a shared buffer.
  virtual scoped_refptr<PlatformSharedBuffer> CreateSharedBuffer(
      size_t num_bytes) = 0;
#endif

  // Multiplexing related methods. They are called from the IO thread only.

  // Called by |message_pipe| so that it receives messages for the given
  // globally unique |pipe_id|. When the connection is established,
  // MessagePipeDispatcher::GotNonTransferableChannel is called with the channel
  // that it can use for sending messages.
  virtual void ConnectMessagePipe(uint64_t pipe_id,
                                  MessagePipeDispatcher* message_pipe) = 0;

  // Called by |message_pipe| when it's closing so that its route can be
  // unregistered.
  // It's ok to call this from a callback (i.e. from OnError or
  // GotNonTransferableChannel).
  virtual void CloseMessagePipe(uint64_t pipe_id,
                                MessagePipeDispatcher* message_pipe) = 0;
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_BROKER_H_
