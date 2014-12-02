// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel_endpoint.h"

#include "base/logging.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/channel_endpoint_client.h"

namespace mojo {
namespace system {

ChannelEndpoint::ChannelEndpoint(ChannelEndpointClient* client,
                                 unsigned client_port,
                                 MessageInTransitQueue* message_queue)
    : client_(client), client_port_(client_port), channel_(nullptr) {
  DCHECK(client_.get() || message_queue);

  if (message_queue)
    channel_message_queue_.Swap(message_queue);
}

bool ChannelEndpoint::EnqueueMessage(scoped_ptr<MessageInTransit> message) {
  DCHECK(message);

  base::AutoLock locker(lock_);

  if (!channel_) {
    // We may reach here if we haven't been attached/run yet.
    // TODO(vtl): We may also reach here if the channel is shut down early for
    // some reason (with live message pipes on it). Ideally, we'd return false
    // (and not enqueue the message), but we currently don't have a way to check
    // this.
    channel_message_queue_.AddMessage(message.Pass());
    return true;
  }

  return WriteMessageNoLock(message.Pass());
}

void ChannelEndpoint::DetachFromClient() {
  {
    base::AutoLock locker(lock_);
    DCHECK(client_.get());
    client_ = nullptr;

    if (!channel_)
      return;
    DCHECK(local_id_.is_valid());
    DCHECK(remote_id_.is_valid());
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

  while (!channel_message_queue_.IsEmpty()) {
    LOG_IF(WARNING, !WriteMessageNoLock(channel_message_queue_.GetMessage()))
        << "Failed to write enqueue message to channel";
  }

  if (!client_.get()) {
    channel_->DetachEndpoint(this, local_id_, remote_id_);
    channel_ = nullptr;
    local_id_ = ChannelEndpointId();
    remote_id_ = ChannelEndpointId();
  }
}

void ChannelEndpoint::OnReadMessage(scoped_ptr<MessageInTransit> message) {
  scoped_refptr<ChannelEndpointClient> client;
  unsigned client_port;
  {
    base::AutoLock locker(lock_);
    DCHECK(channel_);
    if (!client_.get()) {
      // This isn't a failure per se. (It just means that, e.g., the other end
      // of the message point closed first.)
      return;
    }

    // Take a ref, and call |OnReadMessage()| outside the lock.
    client = client_;
    client_port = client_port_;
  }

  client->OnReadMessage(client_port, message.Pass());
}

void ChannelEndpoint::DetachFromChannel() {
  scoped_refptr<ChannelEndpointClient> client;
  unsigned client_port = 0;
  {
    base::AutoLock locker(lock_);

    if (client_.get()) {
      // Take a ref, and call |OnDetachFromChannel()| outside the lock.
      client = client_;
      client_port = client_port_;
    }

    // |channel_| may already be null if we already detached from the channel in
    // |DetachFromClient()| by calling |Channel::DetachEndpoint()| (and there
    // are racing detaches).
    if (channel_) {
      DCHECK(local_id_.is_valid());
      DCHECK(remote_id_.is_valid());
      channel_ = nullptr;
      local_id_ = ChannelEndpointId();
      remote_id_ = ChannelEndpointId();
    }
  }

  if (client.get())
    client->OnDetachFromChannel(client_port);
}

ChannelEndpoint::~ChannelEndpoint() {
  DCHECK(!client_.get());
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
