// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_MESSAGE_PIPE_DISPATCHER_H_
#define MOJO_CORE_MESSAGE_PIPE_DISPATCHER_H_

#include <stdint.h>

#include <memory>
#include <queue>

#include "base/macros.h"
#include "base/optional.h"
#include "mojo/core/atomic_flag.h"
#include "mojo/core/dispatcher.h"
#include "mojo/core/ports/port_ref.h"
#include "mojo/core/ports/slot_ref.h"
#include "mojo/core/watcher_set.h"

namespace mojo {
namespace core {

class NodeController;

class MessagePipeDispatcher : public Dispatcher {
 public:
  // Constructs a MessagePipeDispatcher permanently tied to a specific port.
  // |connected| must indicate the state of the port at construction time; if
  // the port is initialized with a peer, |connected| must be true. Otherwise it
  // must be false.
  //
  // A MessagePipeDispatcher may not be transferred while in a disconnected
  // state, and one can never return to a disconnected once connected.
  //
  // |pipe_id| is a unique identifier which can be used to track pipe endpoints
  // as they're passed around. |endpoint| is either 0 or 1 and again is only
  // used for tracking pipes (one side is always 0, the other is always 1.)
  MessagePipeDispatcher(NodeController* node_controller,
                        const ports::PortRef& port,
                        uint64_t pipe_id,
                        int endpoint);

  // Same as above but binds the dispatcher to a non-default port slot.
  MessagePipeDispatcher(NodeController* node_controller,
                        const ports::SlotRef& slot,
                        uint64_t pipe_id,
                        int endpoint);

  // Fuses this pipe with |other|. Returns |true| on success or |false| on
  // failure. Regardless of the return value, both dispatchers are closed by
  // this call.
  bool Fuse(MessagePipeDispatcher* other);

  // Dispatcher:
  Type GetType() const override;
  MojoResult Close() override;
  MojoResult WriteMessage(
      std::unique_ptr<ports::UserMessageEvent> message) override;
  MojoResult ReadMessage(
      std::unique_ptr<ports::UserMessageEvent>* message) override;
  MojoResult SetQuota(MojoQuotaType type, uint64_t limit) override;
  MojoResult QueryQuota(MojoQuotaType type,
                        uint64_t* limit,
                        uint64_t* usage) override;
  HandleSignalsState GetHandleSignalsState() const override;
  MojoResult AddWatcherRef(const scoped_refptr<WatcherDispatcher>& watcher,
                           uintptr_t context) override;
  MojoResult RemoveWatcherRef(WatcherDispatcher* watcher,
                              uintptr_t context) override;
  void StartSerialize(uint32_t* num_bytes,
                      uint32_t* num_ports,
                      uint32_t* num_handles) override;
  bool EndSerialize(void* destination,
                    ports::UserMessageEvent::PortAttachment* ports,
                    PlatformHandle* handles) override;
  bool BeginTransit() override;
  void CompleteTransitAndClose() override;
  void CancelTransit() override;

  static scoped_refptr<Dispatcher> Deserialize(
      const void* data,
      size_t num_bytes,
      const ports::UserMessageEvent::PortAttachment* ports,
      size_t num_ports,
      PlatformHandle* handles,
      size_t num_handles);

  // Acquires the local MessagePipeDispatcher for this object's peer endpoint,
  // iff both endpoints have always lived in the same process. Returns null
  // otherwise.
  scoped_refptr<MessagePipeDispatcher> GetLocalPeer();

  // Sets the local peer dispatcher. Only set upon message pipe creation, and
  // only remains set until either of the endpoints is transferred to another
  // process.
  void SetLocalPeer(scoped_refptr<MessagePipeDispatcher> peer);

  // Re-binds this MessagePipeDispatcher to a different SlotRef from the one
  // over which it was constructed.
  void BindToSlot(const ports::SlotRef& slot_ref);

 private:
  class SlotObserverThunk;
  friend class SlotObserverThunk;

  ~MessagePipeDispatcher() override;

  MojoResult CloseNoLock();
  HandleSignalsState GetHandleSignalsStateNoLock() const;
  void OnSlotStatusChanged();

  // These are safe to access from any thread without locking.
  NodeController* const node_controller_;
  const uint64_t pipe_id_;
  const int endpoint_;

  // Guards access to all the fields below.
  mutable base::Lock signal_lock_;

  ports::SlotRef slot_;

  // This is not the same is |port_transferred_|. It's only held true between
  // BeginTransit() and Complete/CancelTransit().
  AtomicFlag in_transit_;

  scoped_refptr<MessagePipeDispatcher> local_peer_;
  bool port_transferred_ = false;
  AtomicFlag port_closed_;
  WatcherSet watchers_;
  base::Optional<uint64_t> receive_queue_length_limit_;
  base::Optional<uint64_t> receive_queue_memory_size_limit_;

  DISALLOW_COPY_AND_ASSIGN(MessagePipeDispatcher);
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_MESSAGE_PIPE_DISPATCHER_H_
