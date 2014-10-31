// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel_endpoint.h"

#include "base/logging.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/message_pipe.h"
#include "mojo/edk/system/transport_data.h"

namespace mojo {
namespace system {

ChannelEndpoint::ChannelEndpoint(MessagePipe* message_pipe,
                                 unsigned port,
                                 MessageInTransitQueue* message_queue)
    : message_pipe_(message_pipe), port_(port), channel_(nullptr) {
  DCHECK(message_pipe_.get() || message_queue);
  DCHECK(port_ == 0 || port_ == 1);

  if (message_queue)
    paused_message_queue_.Swap(message_queue);
}

bool ChannelEndpoint::EnqueueMessage(scoped_ptr<MessageInTransit> message) {
  DCHECK(message);

  base::AutoLock locker(lock_);

  if (!channel_ || !remote_id_.is_valid()) {
    // We may reach here if we haven't been attached or run yet.
    // TODO(vtl): We may also reach here if the channel is shut down early for
    // some reason (with live message pipes on it). We can't check |state_| yet,
    // until it's protected under lock, but in this case we should return false
    // (and not enqueue any messages).
    paused_message_queue_.AddMessage(message.Pass());
    return true;
  }

  // TODO(vtl): Currently, this only works in the "running" case.
  DCHECK(remote_id_.is_valid());

  return WriteMessageNoLock(message.Pass());
}

void ChannelEndpoint::DetachFromMessagePipe() {
  {
    base::AutoLock locker(lock_);
    DCHECK(message_pipe_.get());
    message_pipe_ = nullptr;

    if (!channel_)
      return;
    DCHECK(local_id_.is_valid());
    // TODO(vtl): Once we combine "run" into "attach", |remote_id_| should valid
    // here as well.
    channel_->DetachEndpoint(this, local_id_, remote_id_);
    channel_ = nullptr;
    local_id_ = ChannelEndpointId();
    remote_id_ = ChannelEndpointId();
  }
}

void ChannelEndpoint::AttachAndRun(Channel* channel,
                                   ChannelEndpointId local_id,
                                   ChannelEndpointId remote_id) {
  DCHECK(channel);
  DCHECK(local_id.is_valid());
  DCHECK(remote_id.is_valid());

  base::AutoLock locker(lock_);
  DCHECK(!channel_);
  DCHECK(!local_id_.is_valid());
  DCHECK(!remote_id_.is_valid());
  channel_ = channel;
  local_id_ = local_id;
  remote_id_ = remote_id;

  while (!paused_message_queue_.IsEmpty()) {
    LOG_IF(WARNING, !WriteMessageNoLock(paused_message_queue_.GetMessage()))
        << "Failed to write enqueue message to channel";
  }

  if (!message_pipe_.get()) {
    channel_->DetachEndpoint(this, local_id_, remote_id_);
    channel_ = nullptr;
    local_id_ = ChannelEndpointId();
    remote_id_ = ChannelEndpointId();
  }
}

bool ChannelEndpoint::OnReadMessage(
    const MessageInTransit::View& message_view,
    embedder::ScopedPlatformHandleVectorPtr platform_handles) {
  scoped_ptr<MessageInTransit> message(new MessageInTransit(message_view));
  scoped_refptr<MessagePipe> message_pipe;
  unsigned port;
  {
    base::AutoLock locker(lock_);
    DCHECK(channel_);
    if (!message_pipe_.get()) {
      // This isn't a failure per se. (It just means that, e.g., the other end
      // of the message point closed first.)
      return true;
    }

    if (message_view.transport_data_buffer_size() > 0) {
      DCHECK(message_view.transport_data_buffer());
      message->SetDispatchers(TransportData::DeserializeDispatchers(
          message_view.transport_data_buffer(),
          message_view.transport_data_buffer_size(),
          platform_handles.Pass(),
          channel_));
    }

    // Take a ref, and call |EnqueueMessage()| outside the lock.
    message_pipe = message_pipe_;
    port = port_;
  }

  MojoResult result = message_pipe->EnqueueMessage(
      MessagePipe::GetPeerPort(port), message.Pass());
  return (result == MOJO_RESULT_OK);
}

void ChannelEndpoint::OnDisconnect() {
  scoped_refptr<MessagePipe> message_pipe;
  unsigned port;
  {
    base::AutoLock locker(lock_);
    if (!message_pipe_.get())
      return;

    // Take a ref, and call |Close()| outside the lock.
    message_pipe = message_pipe_;
    port = port_;
  }
  message_pipe->Close(port);
}

void ChannelEndpoint::DetachFromChannel() {
  base::AutoLock locker(lock_);
  // This may already be null if we already detached from the channel in
  // |DetachFromMessagePipe()| by calling |Channel::DetachEndpoint()| (and there
  // are racing detaches).
  if (!channel_)
    return;

  DCHECK(local_id_.is_valid());
  // TODO(vtl): Once we combine "run" into "attach", |remote_id_| should valid
  // here as well.
  channel_ = nullptr;
  local_id_ = ChannelEndpointId();
  remote_id_ = ChannelEndpointId();
}

ChannelEndpoint::~ChannelEndpoint() {
  DCHECK(!message_pipe_.get());
  DCHECK(!channel_);
  DCHECK(!local_id_.is_valid());
  DCHECK(!remote_id_.is_valid());
}

bool ChannelEndpoint::WriteMessageNoLock(scoped_ptr<MessageInTransit> message) {
  DCHECK(message);

  lock_.AssertAcquired();

  DCHECK(channel_);
  DCHECK(local_id_.is_valid());
  DCHECK(remote_id_.is_valid());

  message->SerializeAndCloseDispatchers(channel_);
  message->set_source_id(local_id_);
  message->set_destination_id(remote_id_);
  return channel_->WriteMessage(message.Pass());
}

}  // namespace system
}  // namespace mojo
