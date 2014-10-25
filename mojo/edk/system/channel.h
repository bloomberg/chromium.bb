// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_CHANNEL_H_
#define MOJO_EDK_SYSTEM_CHANNEL_H_

#include <stdint.h>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/channel_endpoint.h"
#include "mojo/edk/system/channel_endpoint_id.h"
#include "mojo/edk/system/message_in_transit.h"
#include "mojo/edk/system/message_pipe.h"
#include "mojo/edk/system/raw_channel.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/c/system/types.h"

namespace mojo {

namespace embedder {
class PlatformSupport;
}

namespace system {

class ChannelEndpoint;

// This class is mostly thread-safe. It must be created on an I/O thread.
// |Init()| must be called on that same thread before it becomes thread-safe (in
// particular, before references are given to any other thread) and |Shutdown()|
// must be called on that same thread before destruction. Its public methods are
// otherwise thread-safe. (Many private methods are restricted to the creation
// thread.) It may be destroyed on any thread, in the sense that the last
// reference to it may be released on any thread, with the proviso that
// |Shutdown()| must have been called first (so the pattern is that a "main"
// reference is kept on its creation thread and is released after |Shutdown()|
// is called, but other threads may have temporarily "dangling" references).
//
// Note the lock order (in order of allowable acquisition): |MessagePipe|,
// |ChannelEndpoint|, |Channel|. Thus |Channel| may not call into
// |ChannelEndpoint| with |Channel|'s lock held.
class MOJO_SYSTEM_IMPL_EXPORT Channel
    : public base::RefCountedThreadSafe<Channel>,
      public RawChannel::Delegate {
 public:
  // |platform_support| (typically owned by |Core|) must remain alive until
  // after |Shutdown()| is called.
  explicit Channel(embedder::PlatformSupport* platform_support);

  // This must be called on the creation thread before any other methods are
  // called, and before references to this object are given to any other
  // threads. |raw_channel| should be uninitialized. Returns true on success. On
  // failure, no other methods should be called (including |Shutdown()|).
  bool Init(scoped_ptr<RawChannel> raw_channel);

  // This must be called on the creation thread before destruction (which can
  // happen on any thread).
  void Shutdown();

  // Signals that |Shutdown()| will be called soon (this may be called from any
  // thread, unlike |Shutdown()|). Warnings will be issued if, e.g., messages
  // are written after this is called; other warnings may be suppressed. (This
  // may be called multiple times, or not at all.)
  void WillShutdownSoon();

  // Attaches the given endpoint to this channel and runs it. |is_bootstrap|
  // should be set if and only if it is the first endpoint on the channel. This
  // assigns the endpoint both local and remote IDs. If |is_bootstrap| is set,
  // both are the bootstrap ID (given by |ChannelEndpointId::GetBootstrap()|);
  // if not, it will also send a |kSubtypeChannelAttachAndRunEndpoint| message
  // to the remote side to tell it to create an endpoint as well.
  //
  // (Bootstrapping is symmetric: Both sides attach and run endpoints with
  // |is_bootstrap| set, which establishes the first message pipe across a
  // channel.)
  //
  // This returns the *remote* ID (which will be the bootstrap ID in the
  // bootstrap case, and a "remote ID", i.e., one for which |is_remote()|
  // returns true, otherwise).
  //
  // TODO(vtl): Maybe limit the number of attached message pipes.
  ChannelEndpointId AttachAndRunEndpoint(
      scoped_refptr<ChannelEndpoint> endpoint,
      bool is_bootstrap);

  // This forwards |message| verbatim to |raw_channel_|.
  bool WriteMessage(scoped_ptr<MessageInTransit> message);

  // See |RawChannel::IsWriteBufferEmpty()|.
  // TODO(vtl): Maybe we shouldn't expose this, and instead have a
  // |FlushWriteBufferAndShutdown()| or something like that.
  bool IsWriteBufferEmpty();

  // Removes the given endpoint from this channel (|local_id| and |remote_id|
  // are specified as an optimization; the latter should be an invalid
  // |ChannelEndpointId| if the endpoint is not yet running). Note: If this is
  // called, the |Channel| will *not* call
  // |ChannelEndpoint::DetachFromChannel()|.
  void DetachEndpoint(ChannelEndpoint* endpoint,
                      ChannelEndpointId local_id,
                      ChannelEndpointId remote_id);

  // Takes ownership of an incoming message pipe (i.e., one that was created via
  // a |kSubtypeChannelAttachAndRunEndpoint| message).
  scoped_refptr<MessagePipe> PassIncomingMessagePipe(
      ChannelEndpointId local_id);

  // See |RawChannel::GetSerializedPlatformHandleSize()|.
  size_t GetSerializedPlatformHandleSize() const;

  embedder::PlatformSupport* platform_support() const {
    return platform_support_;
  }

 private:
  friend class base::RefCountedThreadSafe<Channel>;
  ~Channel() override;

  // |RawChannel::Delegate| implementation (only called on the creation thread):
  void OnReadMessage(
      const MessageInTransit::View& message_view,
      embedder::ScopedPlatformHandleVectorPtr platform_handles) override;
  void OnError(Error error) override;

  // Helpers for |OnReadMessage| (only called on the creation thread):
  void OnReadMessageForDownstream(
      const MessageInTransit::View& message_view,
      embedder::ScopedPlatformHandleVectorPtr platform_handles);
  void OnReadMessageForChannel(
      const MessageInTransit::View& message_view,
      embedder::ScopedPlatformHandleVectorPtr platform_handles);

  // Handles "attach and run endpoint" messages.
  bool OnAttachAndRunEndpoint(ChannelEndpointId local_id,
                              ChannelEndpointId remote_id);
  // Handles "remove message pipe endpoint" messages.
  bool OnRemoveMessagePipeEndpoint(ChannelEndpointId local_id,
                                   ChannelEndpointId remote_id);
  // Handles "remove message pipe endpoint ack" messages.
  bool OnRemoveMessagePipeEndpointAck(ChannelEndpointId local_id);

  // Handles errors (e.g., invalid messages) from the remote side. Callable from
  // any thread.
  void HandleRemoteError(const base::StringPiece& error_message);
  // Handles internal errors/failures from the local side. Callable from any
  // thread.
  void HandleLocalError(const base::StringPiece& error_message);

  // Helper to send channel control messages. Returns true on success. Should be
  // called *without* |lock_| held. Callable from any thread.
  bool SendControlMessage(MessageInTransit::Subtype subtype,
                          ChannelEndpointId source_id,
                          ChannelEndpointId destination_id);

  base::ThreadChecker creation_thread_checker_;

  embedder::PlatformSupport* const platform_support_;

  // Note: |MessagePipe|s MUST NOT be used under |lock_|. I.e., |lock_| can only
  // be acquired after |MessagePipe::lock_|, never before. Thus to call into a
  // |MessagePipe|, a reference to the |MessagePipe| should be acquired from
  // |local_id_to_endpoint_map_| under |lock_| and then the lock released.
  base::Lock lock_;  // Protects the members below.

  scoped_ptr<RawChannel> raw_channel_;
  bool is_running_;
  // Set when |WillShutdownSoon()| is called.
  bool is_shutting_down_;

  typedef base::hash_map<ChannelEndpointId, scoped_refptr<ChannelEndpoint>>
      IdToEndpointMap;
  // Map from local IDs to endpoints. If the endpoint is null, this means that
  // we're just waiting for the remove ack before removing the entry.
  IdToEndpointMap local_id_to_endpoint_map_;
  // Note: The IDs generated by this should be checked for existence before use.
  LocalChannelEndpointIdGenerator local_id_generator_;

  typedef base::hash_map<ChannelEndpointId, scoped_refptr<MessagePipe>>
      IdToMessagePipeMap;
  // Map from local IDs to pending/incoming endpoints (i.e., those which do not
  // yet have a dispatcher attached).
  IdToMessagePipeMap incoming_message_pipes_;
  // TODO(vtl): We need to keep track of remote IDs (so that we don't collide
  // if/when we wrap).
  RemoteChannelEndpointIdGenerator remote_id_generator_;

  DISALLOW_COPY_AND_ASSIGN(Channel);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_CHANNEL_H_
