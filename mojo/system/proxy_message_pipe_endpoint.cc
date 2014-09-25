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
    : channel_endpoint_(channel_endpoint),
      is_running_(false),
      is_peer_open_(true) {
}

ProxyMessagePipeEndpoint::ProxyMessagePipeEndpoint(
    ChannelEndpoint* channel_endpoint,
    bool is_peer_open)
    : channel_endpoint_(channel_endpoint),
      is_running_(false),
      is_peer_open_(is_peer_open) {
}

ProxyMessagePipeEndpoint::~ProxyMessagePipeEndpoint() {
  DCHECK(!is_running());
  DCHECK(!is_attached());
}

MessagePipeEndpoint::Type ProxyMessagePipeEndpoint::GetType() const {
  return kTypeProxy;
}

bool ProxyMessagePipeEndpoint::OnPeerClose() {
  DCHECK(is_peer_open_);

  is_peer_open_ = false;

  if (is_attached()) {
    if (!is_running()) {
      // If we're not running yet, we can't be destroyed yet, because we're
      // still waiting for the "run" message from the other side.
      return true;
    }

    Detach();
  }

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

bool ProxyMessagePipeEndpoint::Run() {
  // Assertions about current state:
  DCHECK(is_attached());
  DCHECK(!is_running());

  is_running_ = true;

  if (is_peer_open_)
    return true;  // Stay alive.

  // We were just waiting to die.
  Detach();
  return false;
}

void ProxyMessagePipeEndpoint::OnRemove() {
  Detach();
}

void ProxyMessagePipeEndpoint::Detach() {
  DCHECK(is_attached());

  channel_endpoint_->DetachFromMessagePipe();
  channel_endpoint_ = nullptr;
  is_running_ = false;
}

}  // namespace system
}  // namespace mojo
