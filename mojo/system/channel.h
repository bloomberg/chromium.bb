// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_CHANNEL_H_
#define MOJO_SYSTEM_CHANNEL_H_

#include <stdint.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "mojo/embedder/scoped_platform_handle.h"
#include "mojo/public/c/system/core.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/message_pipe.h"
#include "mojo/system/raw_channel.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

// This class is mostly thread-safe. It must be created on an I/O thread.
// |Init()| must be called on that same thread before it becomes thread-safe (in
// particular, before references are given to any other thread) and |Shutdown()|
// must be called on that same thread before destruction. Its public methods are
// otherwise thread-safe. It may be destroyed on any thread, in the sense that
// the last reference to it may be released on any thread, with the proviso that
// |Shutdown()| must have been called first (so the pattern is that a "main"
// reference is kept on its creation thread and is released after |Shutdown()|
// is called, but other threads may have temporarily "dangling" references).
//
// Note that |MessagePipe| calls into |Channel| and the former's |lock_| must be
// acquired before the latter's. When |Channel| wants to call into a
// |MessagePipe|, it must obtain a reference to the |MessagePipe| (from
// |local_id_to_endpoint_info_map_|) under |Channel::lock_| and then release the
// lock.
//
// Also, care must be taken with respect to references: While a |Channel| has
// references to |MessagePipe|s, |MessagePipe|s (via |ProxyMessagePipeEndpoint|)
// may also have references to |Channel|s. These references are set up by
// calling |AttachMessagePipeEndpoint()|. The reference to |MessagePipe| owned
// by |Channel| must be removed by calling |DetachMessagePipeEndpoint()| (which
// is done by |MessagePipe|/|ProxyMessagePipeEndpoint|, which simultaneously
// removes its reference to |Channel|).
class MOJO_SYSTEM_IMPL_EXPORT Channel
    : public base::RefCountedThreadSafe<Channel>,
      public RawChannel::Delegate {
 public:
  // The first message pipe endpoint attached will have this as its local ID.
  static const MessageInTransit::EndpointId kBootstrapEndpointId = 1;

  Channel();

  // This must be called on the creation thread before any other methods are
  // called, and before references to this object are given to any other
  // threads. |raw_channel| should be uninitialized. Returns true on success. On
  // failure, no other methods should be called (including |Shutdown()|).
  bool Init(scoped_ptr<RawChannel> raw_channel);

  // This must be called on the creation thread before destruction (which can
  // happen on any thread).
  void Shutdown();

  // Attaches the given message pipe/port's endpoint (which must be a
  // |ProxyMessagePipeEndpoint|) to this channel. This assigns it a local ID,
  // which it returns. The first message pipe endpoint attached will always have
  // |kBootstrapEndpointId| as its local ID. (For bootstrapping, this occurs on
  // both sides, so one should use |kBootstrapEndpointId| for the remote ID for
  // the first message pipe across a channel.)
  MessageInTransit::EndpointId AttachMessagePipeEndpoint(
      scoped_refptr<MessagePipe> message_pipe, unsigned port);
  void RunMessagePipeEndpoint(MessageInTransit::EndpointId local_id,
                              MessageInTransit::EndpointId remote_id);

  // Tells the other side of the channel to run a message pipe endpoint (which
  // must already be attached); |local_id| and |remote_id| are relative to this
  // channel (i.e., |local_id| is the other side's remote ID and |remote_id| is
  // its local ID).
  // TODO(vtl): Maybe we should just have a flag argument to
  // |RunMessagePipeEndpoint()| that tells it to do this.
  void RunRemoteMessagePipeEndpoint(MessageInTransit::EndpointId local_id,
                                    MessageInTransit::EndpointId remote_id);

  // This forwards |message| verbatim to |raw_channel_|.
  bool WriteMessage(scoped_ptr<MessageInTransit> message);

  // See |RawChannel::IsWriteBufferEmpty()|.
  // TODO(vtl): Maybe we shouldn't expose this, and instead have a
  // |FlushWriteBufferAndShutdown()| or something like that.
  bool IsWriteBufferEmpty();

  // This removes the message pipe/port's endpoint (with the given local ID,
  // returned by |AttachMessagePipeEndpoint()| from this channel. After this is
  // called, |local_id| may be reused for another message pipe.
  void DetachMessagePipeEndpoint(MessageInTransit::EndpointId local_id);

 private:
  friend class base::RefCountedThreadSafe<Channel>;
  virtual ~Channel();

  // |RawChannel::Delegate| implementation:
  virtual void OnReadMessage(
      const MessageInTransit::View& message_view) OVERRIDE;
  virtual void OnFatalError(FatalError fatal_error) OVERRIDE;

  // Helpers for |OnReadMessage|:
  bool ValidateReadMessage(const MessageInTransit::View& message_view);
  void OnReadMessageForDownstream(const MessageInTransit::View& message_view);
  void OnReadMessageForChannel(const MessageInTransit::View& message_view);

  // Handles errors (e.g., invalid messages) from the remote side.
  void HandleRemoteError(const base::StringPiece& error_message);
  // Handles internal errors/failures from the local side.
  void HandleLocalError(const base::StringPiece& error_message);

  struct EndpointInfo {
    EndpointInfo();
    EndpointInfo(scoped_refptr<MessagePipe> message_pipe, unsigned port);
    ~EndpointInfo();

    scoped_refptr<MessagePipe> message_pipe;
    unsigned port;
  };

  base::ThreadChecker creation_thread_checker_;

  // Note: |MessagePipe|s MUST NOT be used under |lock_|. I.e., |lock_| can only
  // be acquired after |MessagePipe::lock_|, never before. Thus to call into a
  // |MessagePipe|, a reference should be acquired from
  // |local_id_to_endpoint_info_map_| under |lock_| (e.g., by copying the
  // |EndpointInfo|) and then the lock released.
  base::Lock lock_;  // Protects the members below.

  scoped_ptr<RawChannel> raw_channel_;

  typedef base::hash_map<MessageInTransit::EndpointId, EndpointInfo>
      IdToEndpointInfoMap;
  IdToEndpointInfoMap local_id_to_endpoint_info_map_;
  // The next local ID to try (when allocating new local IDs). Note: It should
  // be checked for existence before use.
  MessageInTransit::EndpointId next_local_id_;

  DISALLOW_COPY_AND_ASSIGN(Channel);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_CHANNEL_H_
