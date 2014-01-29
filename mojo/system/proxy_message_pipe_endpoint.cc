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

void ProxyMessagePipeEndpoint::OnPeerClose() {
  DCHECK(is_open_);
  DCHECK(is_peer_open_);

  is_peer_open_ = false;
  MessageInTransit* message =
      MessageInTransit::Create(MessageInTransit::kTypeMessagePipe,
                               MessageInTransit::kSubtypeMessagePipePeerClosed,
                               NULL, 0);
  EnqueueMessageInternal(message, NULL);
}

MojoResult ProxyMessagePipeEndpoint::EnqueueMessage(
    MessageInTransit* message,
    const std::vector<Dispatcher*>* dispatchers) {
  DCHECK(!dispatchers || !dispatchers->empty());

  MojoResult result = CanEnqueueDispatchers(dispatchers);
  if (result != MOJO_RESULT_OK) {
    message->Destroy();
    return result;
  }

  EnqueueMessageInternal(message, dispatchers);
  return MOJO_RESULT_OK;
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

void ProxyMessagePipeEndpoint::Run(MessageInTransit::EndpointId remote_id) {
  // Assertions about arguments:
  DCHECK_NE(remote_id, MessageInTransit::kInvalidEndpointId);

  // Assertions about current state:
  DCHECK(is_attached());
  DCHECK(!is_running());

  AssertConsistentState();
  remote_id_ = remote_id;
  AssertConsistentState();

  for (std::deque<MessageInTransit*>::iterator it =
           paused_message_queue_.begin(); it != paused_message_queue_.end();
       ++it)
    EnqueueMessageInternal(*it, NULL);
  paused_message_queue_.clear();
}

MojoResult ProxyMessagePipeEndpoint::CanEnqueueDispatchers(
    const std::vector<Dispatcher*>* dispatchers) {
  // TODO(vtl): Support sending handles over OS pipes.
  if (dispatchers) {
    NOTIMPLEMENTED();
    return MOJO_RESULT_UNIMPLEMENTED;
  }
  return MOJO_RESULT_OK;
}

// Note: We may have to enqueue messages even when our (local) peer isn't open
// -- it may have been written to and closed immediately, before we were ready.
// This case is handled in |Run()| (which will call us).
void ProxyMessagePipeEndpoint::EnqueueMessageInternal(
    MessageInTransit* message,
    const std::vector<Dispatcher*>* dispatchers) {
  DCHECK(is_open_);

  DCHECK(!dispatchers || !dispatchers->empty());
  // TODO(vtl): We don't actually support sending dispatchers yet. We shouldn't
  // get here due to other checks.
  DCHECK(!dispatchers) << "Not yet implemented";

  if (is_running()) {
    message->set_source_id(local_id_);
    message->set_destination_id(remote_id_);
    // If it fails at this point, the message gets dropped. (This is no
    // different from any other in-transit errors.)
    // Note: |WriteMessage()| will destroy the message even on failure.
    if (!channel_->WriteMessage(message))
      LOG(WARNING) << "Failed to write message to channel";
  } else {
    paused_message_queue_.push_back(message);
  }
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
