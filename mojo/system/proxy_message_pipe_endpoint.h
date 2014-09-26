// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_PROXY_MESSAGE_PIPE_ENDPOINT_H_
#define MOJO_SYSTEM_PROXY_MESSAGE_PIPE_ENDPOINT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/message_pipe_endpoint.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

class ChannelEndpoint;
class LocalMessagePipeEndpoint;
class MessagePipe;

// A |ProxyMessagePipeEndpoint| is an endpoint which delegates everything to a
// |ChannelEndpoint|, which may be co-owned by a |Channel|. Like any
// |MessagePipeEndpoint|, a |ProxyMessagePipeEndpoint| is owned by a
// |MessagePipe|.
//
// For example, a |MessagePipe| with one endpoint local and the other endpoint
// remote consists of a |LocalMessagePipeEndpoint| and a
// |ProxyMessagePipeEndpoint|, with only the local endpoint being accessible via
// a |MessagePipeDispatcher|.
class MOJO_SYSTEM_IMPL_EXPORT ProxyMessagePipeEndpoint
    : public MessagePipeEndpoint {
 public:
  explicit ProxyMessagePipeEndpoint(ChannelEndpoint* channel_endpoint);
  virtual ~ProxyMessagePipeEndpoint();

  // |MessagePipeEndpoint| implementation:
  virtual Type GetType() const OVERRIDE;
  virtual bool OnPeerClose() OVERRIDE;
  virtual void EnqueueMessage(scoped_ptr<MessageInTransit> message) OVERRIDE;

 private:
  scoped_refptr<ChannelEndpoint> channel_endpoint_;

  DISALLOW_COPY_AND_ASSIGN(ProxyMessagePipeEndpoint);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_PROXY_MESSAGE_PIPE_ENDPOINT_H_
