// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/proxy_message_pipe_endpoint.h"

#include <string.h>

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "mojo/system/channel.h"
#include "mojo/system/message_pipe_dispatcher.h"

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
                               NULL, 0, 0);
  EnqueueMessageInternal(message);
}

MojoResult ProxyMessagePipeEndpoint::EnqueueMessage(
    MessageInTransit* message,
    const std::vector<Dispatcher*>* dispatchers) {
  DCHECK(!dispatchers || !dispatchers->empty());

  if (dispatchers)
    SerializeDispatchers(message, dispatchers);

  EnqueueMessageInternal(message);
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
    EnqueueMessageInternal(*it);
  paused_message_queue_.clear();
}

void ProxyMessagePipeEndpoint::SerializeDispatchers(
    MessageInTransit* message,
    const std::vector<Dispatcher*>* dispatchers) {
  DCHECK(!dispatchers->empty());

  // TODO(vtl)
  LOG(ERROR) << "Sending handles over remote message pipes not yet supported "
                "(closing sent handles)";
  for (size_t i = 0; i < dispatchers->size(); i++)
    (*dispatchers)[i]->CloseNoLock();
}

// Note: We may have to enqueue messages even when our (local) peer isn't open
// -- it may have been written to and closed immediately, before we were ready.
// This case is handled in |Run()| (which will call us).
void ProxyMessagePipeEndpoint::EnqueueMessageInternal(
    MessageInTransit* message) {
  DCHECK(is_open_);

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
