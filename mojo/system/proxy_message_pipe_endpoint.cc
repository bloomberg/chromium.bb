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
                               NULL, 0);
  EnqueueMessageInternal(message, NULL);
}

MojoResult ProxyMessagePipeEndpoint::EnqueueMessage(
    MessageInTransit* message,
    const std::vector<Dispatcher*>* dispatchers) {
  DCHECK(!dispatchers || !dispatchers->empty());

  // No need to preflight if there are no dispatchers.
  if (!dispatchers) {
    EnqueueMessageInternal(message, NULL);
    return MOJO_RESULT_OK;
  }

  std::vector<PreflightDispatcherInfo> preflight_dispatcher_infos;
  MojoResult result = PreflightDispatchers(dispatchers,
                                           &preflight_dispatcher_infos);
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

MojoResult ProxyMessagePipeEndpoint::PreflightDispatchers(
    const std::vector<Dispatcher*>* dispatchers,
    std::vector<PreflightDispatcherInfo>* preflight_dispatcher_infos) {
  DCHECK(!dispatchers || !dispatchers->empty());
  DCHECK(preflight_dispatcher_infos);
  DCHECK(preflight_dispatcher_infos->empty());

  // Size it to fit everything.
  preflight_dispatcher_infos->resize(dispatchers->size());

  // TODO(vtl): We'll begin with limited support for sending message pipe
  // handles. We won't support:
  //  - sending both handles (the |hash_set| below is to detect this case and
  //    fail gracefully);
  //  - sending a handle whose peer is remote.
  base::hash_set<intptr_t> message_pipes;

  for (size_t i = 0; i < dispatchers->size(); i++) {
    Dispatcher* dispatcher = (*dispatchers)[i];
    switch (dispatcher->GetType()) {
      case Dispatcher::kTypeUnknown:
        LOG(ERROR) << "Unknown dispatcher type";
        return MOJO_RESULT_INTERNAL;

      case Dispatcher::kTypeMessagePipe: {
        MessagePipeDispatcher* mp_dispatcher =
            static_cast<MessagePipeDispatcher*>(dispatcher);
        (*preflight_dispatcher_infos)[i].message_pipe =
            mp_dispatcher->GetMessagePipeNoLock();
        DCHECK((*preflight_dispatcher_infos)[i].message_pipe);
        (*preflight_dispatcher_infos)[i].port = mp_dispatcher->GetPortNoLock();

        // Check for unsupported cases (see TODO above).
        bool is_new_element = message_pipes.insert(reinterpret_cast<intptr_t>(
            (*preflight_dispatcher_infos)[i].message_pipe)).second;
        if (!is_new_element) {
          NOTIMPLEMENTED()
              << "Sending both sides of a message pipe not yet supported";
          return MOJO_RESULT_UNIMPLEMENTED;
        }
        // TODO(vtl): Check that peer isn't remote (per above TODO).

        break;
      }

      case Dispatcher::kTypeDataPipeProducer:
        NOTIMPLEMENTED() << "Sending data pipe producers not yet supported";
        return MOJO_RESULT_UNIMPLEMENTED;

      case Dispatcher::kTypeDataPipeConsumer:
        NOTIMPLEMENTED() << "Sending data pipe consumers not yet supported";
        return MOJO_RESULT_UNIMPLEMENTED;

      default:
        LOG(ERROR) << "Invalid or unsupported dispatcher type";
        return MOJO_RESULT_UNIMPLEMENTED;
    }
  }

  // TODO(vtl): Support sending handles over OS pipes.
  NOTIMPLEMENTED();
  return MOJO_RESULT_UNIMPLEMENTED;
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
