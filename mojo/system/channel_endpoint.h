// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_CHANNEL_ENDPOINT_H_
#define MOJO_SYSTEM_CHANNEL_ENDPOINT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/message_in_transit_queue.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

class Channel;
class MessagePipe;

// TODO(vtl): The plan:
//   - (Done.) Move |Channel::Endpoint| to |ChannelEndpoint|. Make it
//     refcounted, and not copyable. Make |Channel| a friend. Make things work.
//   - (Done.) Give |ChannelEndpoint| a lock. The lock order (in order of
//     allowable acquisition) is: |MessagePipe|, |ChannelEndpoint|, |Channel|.
//   - Stop having |Channel| as a friend.
//   - Move logic from |ProxyMessagePipeEndpoint| into |ChannelEndpoint|. Right
//     now, we have to go through lots of contortions to manipulate state owned
//     by |ProxyMessagePipeEndpoint| (in particular, |Channel::Endpoint| doesn't
//     know about the remote ID; the local ID is duplicated in two places).
//     Hollow out |ProxyMessagePipeEndpoint|, and have it just own a reference
//     to |ChannelEndpoint| (hence the refcounting).
//   - In essence, |ChannelEndpoint| becomes the thing that knows about
//     channel-specific aspects of an endpoint (notably local and remote IDs,
//     and knowledge about handshaking), and mediates between the |Channel| and
//     the |MessagePipe|.
//   - In the end state, |Channel| should no longer need to know about
//     |MessagePipe| and ports (but only |ChannelEndpoint|) and
//     |ProxyMessagePipeEndpoint| should no longer need to know about |Channel|
//     (ditto).
//
// Things as they are now, before I change everything (TODO(vtl): update this
// comment appropriately):
//
// Terminology:
//   - "Message pipe endpoint": In the implementation, a |MessagePipe| owns
//     two |MessagePipeEndpoint| objects, one for each port. The
//     |MessagePipeEndpoint| objects are only accessed via the |MessagePipe|
//     (which has the lock), with the additional information of the port
//     number. So as far as the channel is concerned, a message pipe endpoint
//     is a pointer to a |MessagePipe| together with the port number.
//       - The value of |port| in |EndpointInfo| refers to the
//         |ProxyMessagePipeEndpoint| (i.e., the endpoint that is logically on
//         the other side). Messages received by a channel for a message pipe
//         are thus written to the *peer* of this port.
//   - "Attached"/"detached": A message pipe endpoint is attached to a channel
//     if it has a pointer to it. It must be detached before the channel gives
//     up its pointer to it in order to break a reference cycle. (This cycle
//     is needed to allow a channel to be shut down cleanly, without shutting
//     down everything else first.)
//   - "Running" (message pipe endpoint): A message pipe endpoint is running
//     if messages written to it (via some |MessagePipeDispatcher|, to which
//     some |MojoHandle| is assigned) are being transmitted through the
//     channel.
//       - Before a message pipe endpoint is run, it will queue messages.
//       - When a message pipe endpoint is detached from a channel, it is also
//         taken out of the running state. After that point, messages should
//         no longer be written to it.
//   - "Normal" message pipe endpoint (state): The channel itself does not
//     have knowledge of whether a message pipe endpoint has started running
//     yet. It will *receive* messages for a message pipe in either state (but
//     the message pipe endpoint won't *send* messages to the channel if it
//     has not started running).
//   - "Zombie" message pipe endpoint (state): A message pipe endpoint is a
//     zombie if it is still in |local_id_to_endpoint_info_map_|, but the
//     channel is no longer forwarding messages to it (even if it may still be
//     receiving messages for it).
//       - There are various types of zombies, depending on the reason the
//         message pipe endpoint cannot yet be removed.
//       - If the remote side is closed, it will send a "remove" control
//         message. After the channel receives that message (to which it
//         responds with a "remove ack" control message), it knows that it
//         shouldn't receive any more messages for that message pipe endpoint
//         (local ID), but it must wait for the endpoint to detach. (It can't
//         do so without a race, since it can't call into the message pipe
//         under |lock_|.) [TODO(vtl): When I add remotely-allocated IDs,
//         we'll have to remove the |EndpointInfo| from
//         |local_id_to_endpoint_info_map_| -- i.e., remove the local ID,
//         since it's no longer valid and may be reused by the remote side --
//         and keep the |EndpointInfo| alive in some other way.]
//       - If the local side is closed and the message pipe endpoint was
//         already running (so there are no queued messages left to send), it
//         will detach the endpoint, and send a "remove" control message.
//         However, the channel may still receive messages for that endpoint
//         until it receives a "remove ack" control message.
//       - If the local side is closed but the message pipe endpoint was not
//         yet running , the detaching is delayed until after it is run and
//         all the queued messages are sent to the channel. On being detached,
//         things proceed as in one of the above cases. The endpoint is *not*
//         a zombie until it is detached (or a "remove" message is received).
//         [TODO(vtl): Maybe we can get rid of this case? It'd only not yet be
//         running since under the current scheme it wouldn't have a remote ID
//         yet.]
//       - Note that even if the local side is closed, it may still receive a
//         "remove" message from the other side (if the other side is closed
//         simultaneously, and both sides send "remove" messages). In that
//         case, it must still remain alive until it receives the "remove
//         ack" (and it must ack the "remove" message that it received).
class MOJO_SYSTEM_IMPL_EXPORT ChannelEndpoint
    : public base::RefCountedThreadSafe<ChannelEndpoint> {
 public:
  // TODO(vtl): More comments....
  ChannelEndpoint(MessagePipe* message_pipe, unsigned port);

  // Takes messages from the given |MessageInTransitQueue|. This must be called
  // before this object is attached to a channel, and before anyone has a chance
  // to enqueue any messages.
  void TakeMessages(MessageInTransitQueue* message_queue);

  // Methods called by |MessagePipe| (via |ProxyMessagePipeEndpoint|):

  // TODO(vtl): This currently only works if we're "running". We'll move the
  // "paused message queue" here (will this be needed when we have
  // locally-allocated remote IDs?).
  bool EnqueueMessage(scoped_ptr<MessageInTransit> message);

  void DetachFromMessagePipe();

  // Methods called by |Channel|:

  // Called by |Channel| when it takes a reference to this object.
  void AttachToChannel(Channel* channel, MessageInTransit::EndpointId local_id);

  // TODO(vtl): Combine this with |AttachToChannel()|.
  void Run(MessageInTransit::EndpointId remote_id);

  // Called by |Channel| before it gives up its reference to this object.
  void DetachFromChannel();

 private:
  enum State {
    // Attached, possibly running or not.
    STATE_NORMAL,
    // "Zombie" states:
    // Waiting for |DetachMessagePipeEndpoint()| before removing.
    STATE_WAIT_LOCAL_DETACH,
    // Waiting for a |kSubtypeChannelRemoveMessagePipeEndpointAck| before
    // removing.
    STATE_WAIT_REMOTE_REMOVE_ACK,
  };

  // TODO(vtl): This is totally temporary, until this becomes a proper
  // self-contained class. See the plan above.
  friend class Channel;

  friend class base::RefCountedThreadSafe<ChannelEndpoint>;
  ~ChannelEndpoint();

  // Must be called with |lock_| held.
  bool WriteMessageNoLock(scoped_ptr<MessageInTransit> message);

  // TODO(vtl): Move these under lock.
  State state_;
  // TODO(vtl): When moved under lock, this can/should be made a raw pointer.
  scoped_refptr<MessagePipe> message_pipe_;
  unsigned port_;

  // TODO(vtl): Move the things above under lock.
  // Protects the members below.
  base::Lock lock_;

  // |channel_| must be alive whenever this is non-null. Before the |channel_|
  // gives up its reference to this object, it will call |DetachFromChannel()|.
  Channel* channel_;
  MessageInTransit::EndpointId local_id_;
  MessageInTransit::EndpointId remote_id_;

  // This queue is used before we're running on a channel and ready to send
  // messages.
  MessageInTransitQueue paused_message_queue_;

  DISALLOW_COPY_AND_ASSIGN(ChannelEndpoint);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_CHANNEL_ENDPOINT_H_
