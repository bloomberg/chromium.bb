// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_BROKER_STATE_H_
#define MOJO_EDK_SYSTEM_BROKER_STATE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/broker.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {
class ChildBrokerHost;
class RoutedRawChannel;

// Common broker state that has to live in a parent process. There is one
// instance of this class in the parent process. This class implements the
// Broker interface for use by code in the parent process as well.
class MOJO_SYSTEM_IMPL_EXPORT BrokerState : NON_EXPORTED_BASE(public Broker) {
 public:
  static BrokerState* GetInstance();

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

  // Called by ChildBrokerHost on construction and destruction.
  void ChildBrokerHostCreated(ChildBrokerHost* child_broker_host);
  void ChildBrokerHostDestructed(ChildBrokerHost* child_broker_host);

  // These are called by ChildBrokerHost as they dispatch IPCs from ChildBroker.
  // They are called on the IO thread.
  void HandleConnectMessagePipe(ChildBrokerHost* pipe_process,
                                uint64_t pipe_id);
  void HandleCancelConnectMessagePipe(uint64_t pipe_id);

 private:
  friend struct base::DefaultSingletonTraits<BrokerState>;

  BrokerState();
  ~BrokerState() override;

  // Checks if there's a direct channel between the two processes, and if not
  // creates one and tells them about it.
  void EnsureProcessesConnected(base::ProcessId pid1, base::ProcessId pid2);

  // Callback when a RoutedRawChannel is destroyed for cleanup.
  // Called on the IO thread.
  void ChannelDestructed(RoutedRawChannel* channel);

  // Helper method to connect the given MessagePipe to the channel.
  void AttachMessagePipe(MessagePipeDispatcher* message_pipe,
                         uint64_t pipe_id,
                         RoutedRawChannel* raw_channel);

#if defined(OS_WIN)
  // Used in the parent (unsandboxed) process to hold a mapping between HANDLES
  // and tokens. When a child process wants to send a HANDLE to another process,
  // it exchanges it to a token and then the other process exchanges that token
  // back to a HANDLE.
  base::Lock token_map_lock_;
  base::hash_map<uint64_t, HANDLE> token_map_;
#endif

  // For pending connects originiating in this process.
  // Only accessed on the IO thread.
  base::hash_map<uint64_t, MessagePipeDispatcher*> pending_connects_;

  // For connected message pipes in this process. This is needed so that when a
  // MessagePipeDispatcher is closed we can remove the route for the
  // corresponding RoutedRawChannel.
  // Only accessed on the IO thread.
  base::hash_map<MessagePipeDispatcher*, RoutedRawChannel*> connected_pipes_;

  base::Lock lock_;  // Guards access to below.

  base::hash_map<uint64_t, ChildBrokerHost*> pending_child_connects_;

  // Each entry is an std::pair of ints of processe IDs that have
  // RoutedRawChannel objects between them. The pair always has the smaller
  // process id value first.
  // For now, we don't reap connections if there are no more routes between two
  // processes.
  base::hash_set<std::pair<base::ProcessId, base::ProcessId>>
      connected_processes_;

  base::hash_map<base::ProcessId, ChildBrokerHost*> child_processes_;

  // Used for message pipes in the same process.
  RoutedRawChannel* in_process_pipes_channel1_;
  RoutedRawChannel* in_process_pipes_channel2_;

  DISALLOW_COPY_AND_ASSIGN(BrokerState);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_BROKER_STATE_H_
