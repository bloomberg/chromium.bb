// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_PROXY_MESSAGE_PIPE_ENDPOINT_H_
#define MOJO_SYSTEM_PROXY_MESSAGE_PIPE_ENDPOINT_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/message_in_transit_queue.h"
#include "mojo/system/message_pipe_endpoint.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

class ChannelEndpoint;
class LocalMessagePipeEndpoint;
class MessagePipe;

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
  // Constructs a |ProxyMessagePipeEndpoint| that replaces the given
  // |LocalMessagePipeEndpoint| (which this constructor will close), taking its
  // message queue's contents. This is done when transferring a message pipe
  // handle over a remote message pipe.
  ProxyMessagePipeEndpoint(
      LocalMessagePipeEndpoint* local_message_pipe_endpoint,
      bool is_peer_open);
  virtual ~ProxyMessagePipeEndpoint();

  // |MessagePipeEndpoint| implementation:
  virtual Type GetType() const OVERRIDE;
  virtual bool OnPeerClose() OVERRIDE;
  virtual void EnqueueMessage(scoped_ptr<MessageInTransit> message) OVERRIDE;
  virtual void Attach(ChannelEndpoint* channel_endpoint) OVERRIDE;
  virtual bool Run() OVERRIDE;
  virtual void OnRemove() OVERRIDE;

 private:
  void Detach();

  // TODO(vtl): Get rid of these.
  bool is_attached() const { return !!channel_endpoint_.get(); }
  bool is_running() const { return is_running_; }

  // This should only be set if we're attached.
  scoped_refptr<ChannelEndpoint> channel_endpoint_;

  // TODO(vtl): Get rid of this.
  bool is_running_;

  bool is_peer_open_;

  // This queue is only used while we're detached, to store messages while we're
  // not ready to send them yet.
  MessageInTransitQueue paused_message_queue_;

  DISALLOW_COPY_AND_ASSIGN(ProxyMessagePipeEndpoint);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_PROXY_MESSAGE_PIPE_ENDPOINT_H_
