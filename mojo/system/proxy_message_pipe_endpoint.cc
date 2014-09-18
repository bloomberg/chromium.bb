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

ProxyMessagePipeEndpoint::ProxyMessagePipeEndpoint()
    : is_running_(false), is_peer_open_(true) {
}

ProxyMessagePipeEndpoint::ProxyMessagePipeEndpoint(
    LocalMessagePipeEndpoint* local_message_pipe_endpoint,
    bool is_peer_open)
    : is_running_(false),
      is_peer_open_(is_peer_open),
      paused_message_queue_(MessageInTransitQueue::PassContents(),
                            local_message_pipe_endpoint->message_queue()) {
  local_message_pipe_endpoint->Close();
}

ProxyMessagePipeEndpoint::~ProxyMessagePipeEndpoint() {
  DCHECK(!is_running());
  DCHECK(!is_attached());
  DCHECK(paused_message_queue_.IsEmpty());
}

MessagePipeEndpoint::Type ProxyMessagePipeEndpoint::GetType() const {
  return kTypeProxy;
}

bool ProxyMessagePipeEndpoint::OnPeerClose() {
  DCHECK(is_peer_open_);

  is_peer_open_ = false;

  // If our outgoing message queue isn't empty, we shouldn't be destroyed yet.
  if (!paused_message_queue_.IsEmpty())
    return true;

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
  if (is_running()) {
    DCHECK(channel_endpoint_.get());
    LOG_IF(WARNING, !channel_endpoint_->EnqueueMessage(message.Pass()))
        << "Failed to write enqueue message to channel";
  } else {
    paused_message_queue_.AddMessage(message.Pass());
  }
}

void ProxyMessagePipeEndpoint::Attach(ChannelEndpoint* channel_endpoint) {
  DCHECK(channel_endpoint);
  DCHECK(!is_attached());
  channel_endpoint_ = channel_endpoint;
}

bool ProxyMessagePipeEndpoint::Run() {
  // Assertions about current state:
  DCHECK(is_attached());
  DCHECK(!is_running());

  is_running_ = true;

  while (!paused_message_queue_.IsEmpty()) {
    LOG_IF(
        WARNING,
        !channel_endpoint_->EnqueueMessage(paused_message_queue_.GetMessage()))
        << "Failed to write enqueue message to channel";
  }

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
  channel_endpoint_ = NULL;
  is_running_ = false;
  paused_message_queue_.Clear();
}

}  // namespace system
}  // namespace mojo
