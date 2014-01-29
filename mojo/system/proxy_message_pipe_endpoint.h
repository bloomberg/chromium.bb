// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_PROXY_MESSAGE_PIPE_ENDPOINT_H_
#define MOJO_SYSTEM_PROXY_MESSAGE_PIPE_ENDPOINT_H_

#include <stdint.h>

#include <deque>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/system/core.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/message_pipe_endpoint.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

class Channel;

// A |ProxyMessagePipeEndpoint| connects an end of a |MessagePipe| to a
// |Channel|, over which it transmits and receives data (to/from another
// |ProxyMessagePipeEndpoint|). So a |MessagePipe| with one endpoint local and
// the other endpoint remote consists of a |LocalMessagePipeEndpoint| and a
// |ProxyMessagePipeEndpoint|, with only the local endpoint being accessible via
// a |MessagePipeDispatcher|.
//
// Like any |MessagePipeEndpoint|, a |ProxyMessagePipeEndpoint| is owned by a
// |MessagePipe|.
//  - A |ProxyMessagePipeEndpoint| starts out *detached*, i.e., not associated
//    to any |Channel|. When *attached*, it gets a reference to a |Channel| and
//    is assigned a local ID. A |ProxyMessagePipeEndpoint| must be detached
//    before destruction; this is done inside |Close()|.
//  - When attached, a |ProxyMessagePipeEndpoint| starts out not running. When
//    run, it gets a remote ID.
class MOJO_SYSTEM_IMPL_EXPORT ProxyMessagePipeEndpoint
    : public MessagePipeEndpoint {
 public:
  ProxyMessagePipeEndpoint();
  virtual ~ProxyMessagePipeEndpoint();

  // |MessagePipeEndpoint| implementation:
  virtual void Close() OVERRIDE;
  virtual void OnPeerClose() OVERRIDE;
  virtual MojoResult EnqueueMessage(
      MessageInTransit* message,
      const std::vector<Dispatcher*>* dispatchers) OVERRIDE;
  virtual void Attach(scoped_refptr<Channel> channel,
                      MessageInTransit::EndpointId local_id) OVERRIDE;
  virtual void Run(MessageInTransit::EndpointId remote_id) OVERRIDE;

 private:
  bool is_attached() const {
    return !!channel_.get();
  }

  bool is_running() const {
    return remote_id_ != MessageInTransit::kInvalidEndpointId;
  }

  MojoResult CanEnqueueDispatchers(const std::vector<Dispatcher*>* dispatchers);
  // |dispatchers| should be non-null only if it's nonempty, in which case the
  // dispatchers should have been preflighted by |CanEnqueueDispatchers()|.
  void EnqueueMessageInternal(MessageInTransit* message,
                              const std::vector<Dispatcher*>* dispatchers);

#ifdef NDEBUG
  void AssertConsistentState() const {}
#else
  void AssertConsistentState() const;
#endif

  // This should only be set if we're attached.
  scoped_refptr<Channel> channel_;

  // |local_id_| should be set to something other than
  // |MessageInTransit::kInvalidEndpointId| when we're attached.
  MessageInTransit::EndpointId local_id_;

  // |remote_id_| being set to anything other than
  // |MessageInTransit::kInvalidEndpointId| indicates that we're "running",
  // i.e., actively able to send messages. We should only ever be running if
  // we're attached.
  MessageInTransit::EndpointId remote_id_;

  bool is_open_;
  bool is_peer_open_;

  // This queue is only used while we're detached, to store messages while we're
  // not ready to send them yet.
  std::deque<MessageInTransit*> paused_message_queue_;

  DISALLOW_COPY_AND_ASSIGN(ProxyMessagePipeEndpoint);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_PROXY_MESSAGE_PIPE_ENDPOINT_H_
