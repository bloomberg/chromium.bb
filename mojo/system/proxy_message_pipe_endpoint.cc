// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/proxy_message_pipe_endpoint.h"

#include <string.h>

#include "base/logging.h"
#include "mojo/system/channel_endpoint.h"
#include "mojo/system/local_message_pipe_endpoint.h"
#include "mojo/system/message_pipe_dispatcher.h"

namespace mojo {
namespace system {

ProxyMessagePipeEndpoint::ProxyMessagePipeEndpoint(
    ChannelEndpoint* channel_endpoint)
    : channel_endpoint_(channel_endpoint) {
}

ProxyMessagePipeEndpoint::~ProxyMessagePipeEndpoint() {
  channel_endpoint_->DetachFromMessagePipe();
}

MessagePipeEndpoint::Type ProxyMessagePipeEndpoint::GetType() const {
  return kTypeProxy;
}

bool ProxyMessagePipeEndpoint::OnPeerClose() {
  return false;
}

// Note: We may have to enqueue messages even when our (local) peer isn't open
// -- it may have been written to and closed immediately, before we were ready.
// This case is handled in |Run()| (which will call us).
void ProxyMessagePipeEndpoint::EnqueueMessage(
    scoped_ptr<MessageInTransit> message) {
  DCHECK(channel_endpoint_.get());
  LOG_IF(WARNING, !channel_endpoint_->EnqueueMessage(message.Pass()))
      << "Failed to write enqueue message to channel";
}

}  // namespace system
}  // namespace mojo
