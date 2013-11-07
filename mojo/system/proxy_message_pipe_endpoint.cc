// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/proxy_message_pipe_endpoint.h"

#include <string.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "mojo/system/channel.h"

namespace mojo {
namespace system {

ProxyMessagePipeEndpoint::ProxyMessagePipeEndpoint()
    : local_id_(MessageInTransit::kInvalidEndpointId),
      remote_id_(MessageInTransit::kInvalidEndpointId),
      is_open_(true),
      is_peer_open_(true) {
}

ProxyMessagePipeEndpoint::~ProxyMessagePipeEndpoint() {
  DCHECK(!is_running());
  DCHECK(!is_attached());
  AssertConsistentState();
  DCHECK(paused_message_queue_.empty());
}

void ProxyMessagePipeEndpoint::Close() {
  DCHECK(is_open_);
  is_open_ = false;

  DCHECK(is_attached());
  channel_->DetachMessagePipeEndpoint(local_id_);
  channel_ = NULL;
  local_id_ = MessageInTransit::kInvalidEndpointId;
  remote_id_ = MessageInTransit::kInvalidEndpointId;

  for (std::deque<MessageInTransit*>::iterator it =
           paused_message_queue_.begin();
       it != paused_message_queue_.end();
       ++it) {
    (*it)->Destroy();
  }
  paused_message_queue_.clear();
}

bool ProxyMessagePipeEndpoint::OnPeerClose() {
  DCHECK(is_open_);
  DCHECK(is_peer_open_);

  is_peer_open_ = false;
  if (EnqueueMessage(MessageInTransit::Create(
          MessageInTransit::kTypeMessagePipe,
          MessageInTransit::kSubtypeMessagePipePeerClosed,
          NULL, 0)) != MOJO_RESULT_OK) {
    // TODO(vtl): Do something more sensible on error here?
    LOG(WARNING) << "Failed to send peer closed control message";
  }

  // Return false -- to indicate that we should be destroyed -- if no messages
  // are still enqueued. (Messages may still be enqueued if we're not running
  // yet, but our peer was closed.)
  return !paused_message_queue_.empty();
}

MojoResult ProxyMessagePipeEndpoint::EnqueueMessage(MessageInTransit* message) {
  DCHECK(is_open_);
  // If our (local) peer isn't open, we should only be enqueueing our own
  // control messages.
  DCHECK(is_peer_open_ ||
         (message->type() == MessageInTransit::kTypeMessagePipe));

  MojoResult rv = MOJO_RESULT_OK;

  if (is_running()) {
    message->set_source_id(local_id_);
    message->set_destination_id(remote_id_);
    if (!channel_->WriteMessage(message))
      rv = MOJO_RESULT_FAILED_PRECONDITION;
  } else {
    paused_message_queue_.push_back(message);
  }

  return rv;
}

void ProxyMessagePipeEndpoint::Attach(scoped_refptr<Channel> channel,
                                      MessageInTransit::EndpointId local_id) {
  DCHECK(channel.get());
  DCHECK_NE(local_id, MessageInTransit::kInvalidEndpointId);

  DCHECK(!is_attached());

  AssertConsistentState();
  channel_ = channel;
  local_id_ = local_id;
  AssertConsistentState();
}

bool ProxyMessagePipeEndpoint::Run(MessageInTransit::EndpointId remote_id) {
  // Assertions about arguments:
  DCHECK_NE(remote_id, MessageInTransit::kInvalidEndpointId);

  // Assertions about current state:
  DCHECK(is_attached());
  DCHECK(!is_running());

  AssertConsistentState();
  remote_id_ = remote_id;
  AssertConsistentState();

  MojoResult result = MOJO_RESULT_OK;
  for (std::deque<MessageInTransit*>::iterator it =
           paused_message_queue_.begin();
       it != paused_message_queue_.end();
       ++it) {
    result = EnqueueMessage(*it);
    if (result != MOJO_RESULT_OK) {
      // TODO(vtl): Do something more sensible on error here?
      LOG(WARNING) << "Failed to send message";
    }
  }
  paused_message_queue_.clear();

  // If the peer is not open, we should return false since we should be
  // destroyed.
  return is_peer_open_;
}

#ifndef NDEBUG
void ProxyMessagePipeEndpoint::AssertConsistentState() const {
  if (is_attached()) {
    DCHECK_NE(local_id_, MessageInTransit::kInvalidEndpointId);
  } else {  // Not attached.
    DCHECK_EQ(local_id_, MessageInTransit::kInvalidEndpointId);
    DCHECK_EQ(remote_id_, MessageInTransit::kInvalidEndpointId);
  }
}
#endif

}  // namespace system
}  // namespace mojo
