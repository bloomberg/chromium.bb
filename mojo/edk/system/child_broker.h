// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_CHILD_BROKER_H_
#define MOJO_EDK_SYSTEM_CHILD_BROKER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock_impl.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/broker.h"
#include "mojo/edk/system/message_in_transit_queue.h"
#include "mojo/edk/system/routed_raw_channel.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {
struct BrokerMessage;

// An implementation of Broker used in (sandboxed) child processes. It talks
// over sync IPCs to the (unsandboxed) parent process (specifically,
// ChildBrokerHost) to convert handles to tokens and vice versa. It also sends
// async messages to handle message pipe multiplexing.
class MOJO_SYSTEM_IMPL_EXPORT ChildBroker
    : public Broker, public RawChannel::Delegate {
 public:
  static ChildBroker* GetInstance();

  // Passes the platform handle that is used to talk to ChildBrokerHost.
  void SetChildBrokerHostHandle(ScopedPlatformHandle handle);

  // Broker implementation:
#if defined(OS_WIN)
  void CreatePlatformChannelPair(ScopedPlatformHandle* server,
                                 ScopedPlatformHandle* client) override;
  void HandleToToken(const PlatformHandle* platform_handles,
                     size_t count,
                     uint64_t* tokens) override;
  void TokenToHandle(const uint64_t* tokens,
                     size_t count,
                     PlatformHandle* handles) override;
#else
  scoped_refptr<PlatformSharedBuffer> CreateSharedBuffer(
      size_t num_bytes) override;
#endif

  void ConnectMessagePipe(uint64_t pipe_id,
                          MessagePipeDispatcher* message_pipe) override;
  void CloseMessagePipe(uint64_t pipe_id,
                        MessagePipeDispatcher* message_pipe) override;

 private:
  friend struct base::DefaultSingletonTraits<ChildBroker>;

  ChildBroker();
  ~ChildBroker() override;

  // RawChannel::Delegate implementation:
  void OnReadMessage(
      const MessageInTransit::View& message_view,
      ScopedPlatformHandleVectorPtr platform_handles) override;
  void OnError(Error error) override;

  // Callback for when a RoutedRawChannel is destroyed for cleanup.
  void ChannelDestructed(RoutedRawChannel* channel);

  // Sends a message over |parent_async_channel_|, queueing it if necessary.
  void WriteAsyncMessage(scoped_ptr<MessageInTransit> message);

  // Initializes |parent_async_channel_|.
  void InitAsyncChannel(ScopedPlatformHandle parent_async_channel_handle);

  // Helper method to connect the given MessagePipe to the channel.
  void AttachMessagePipe(MessagePipeDispatcher* message_pipe,
                         uint64_t pipe_id,
                         RoutedRawChannel* raw_channel);

#if defined(OS_WIN)
  // Helper method to write the given message and read back the result.
  bool WriteAndReadResponse(BrokerMessage* message,
                            void* response,
                            uint32_t response_size);

  void CreatePlatformChannelPairNoLock(ScopedPlatformHandle* server,
                                       ScopedPlatformHandle* client);

#else
  // Will fully write |message|, then read back some number of handles. Returns
  // false if the write or read failed, or if there were no handles received.
  bool WriteAndReadHandles(BrokerMessage* message,
                           std::deque<PlatformHandle>* handles);
#endif

  // Guards access to |parent_sync_channel_|.
  // We use LockImpl instead of Lock because the latter adds thread checking
  // that we don't want (since we lock in the constructor and unlock on another
  // thread.
  base::internal::LockImpl sync_channel_lock_;

  // Pipe used for communication to the parent process. We use a pipe directly
  // instead of bindings or RawChannel because we need to send synchronous
  // messages with replies from any thread.
  ScopedPlatformHandle parent_sync_channel_;

  // RawChannel used for asynchronous communication to and from the parent
  // process. Since these messages are bidirectional, we can't use
  // |parent_sync_channel_| which is only used for sync messages to the parent.
  // However since the messages are asynchronous, we can use RawChannel for
  // convenience instead of writing and reading from pipes manually. Although it
  // would be convenient, we don't use Mojo IPC because it would be a layering
  // violation (and cirular dependency) if the system layer depended on
  // bindings.
  RoutedRawChannel* parent_async_channel_;

  // Queue of messages to |parent_async_channel_| that are sent before it is
  // created.
  MessageInTransitQueue async_channel_queue_;

  // The following members are only used on the IO thread, so they don't need to
  // be used under a lock.

  // Maps from routing ids to the MessagePipeDispatcher that have requested to
  // connect to them. When the parent replies with which process they should be
  // connected to, they will migrate to |connected_pipes_|.
  base::hash_map<uint64_t, MessagePipeDispatcher*> pending_connects_;

  // These are MessagePipeDispatchers that are connected to other MPDs in this
  // process, but they both connected before we had parent_sync_channel_ so we
  // delayed connecting them.
  base::hash_map<uint64_t, MessagePipeDispatcher*> pending_inprocess_connects_;

  // Map from MessagePipeDispatcher to its RoutedRawChannel. This is needed so
  // that when a MessagePipeDispatcher is closed we can remove the route for the
  // corresponding RoutedRawChannel.
  // Note the key is MessagePipeDispatcher*, instead of pipe_id, because when
  // the two pipes are in the same process they will have one pipe_id but be
  // connected to the two RoutedRawChannel objects below.
  base::hash_map<MessagePipeDispatcher*, RoutedRawChannel*> connected_pipes_;

  // Holds a map of all the RoutedRawChannel that connect this child process to
  // any other process. The key is the peer process'd pid.
  base::hash_map<base::ProcessId, RoutedRawChannel*> channels_;

  // Used for message pipes in the same process.
  RoutedRawChannel* in_process_pipes_channel1_;
  RoutedRawChannel* in_process_pipes_channel2_;

  DISALLOW_COPY_AND_ASSIGN(ChildBroker);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_CHILD_BROKER_H_
