// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/channel.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"

namespace mojo {
namespace system {

COMPILE_ASSERT(Channel::kBootstrapEndpointId !=
                   MessageInTransit::kInvalidEndpointId,
               kBootstrapEndpointId_is_invalid);

STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::EndpointId
    Channel::kBootstrapEndpointId;

Channel::EndpointInfo::EndpointInfo() {
}

Channel::EndpointInfo::EndpointInfo(scoped_refptr<MessagePipe> message_pipe,
                                    unsigned port)
    : message_pipe(message_pipe),
      port(port) {
}

Channel::EndpointInfo::~EndpointInfo() {
}

Channel::Channel()
    : next_local_id_(kBootstrapEndpointId) {
}

bool Channel::Init(embedder::ScopedPlatformHandle handle) {
  DCHECK(creation_thread_checker_.CalledOnValidThread());

  // No need to take |lock_|, since this must be called before this object
  // becomes thread-safe.
  DCHECK(!raw_channel_.get());

  raw_channel_.reset(
      RawChannel::Create(handle.Pass(), this, base::MessageLoop::current()));
  if (!raw_channel_->Init()) {
    raw_channel_.reset();
    return false;
  }

  return true;
}

void Channel::Shutdown() {
  DCHECK(creation_thread_checker_.CalledOnValidThread());

  base::AutoLock locker(lock_);
  DCHECK(raw_channel_.get());
  raw_channel_->Shutdown();
  raw_channel_.reset();

  // TODO(vtl): Should I clear |local_id_to_endpoint_info_map_|? Or assert that
  // it's empty?
}

MessageInTransit::EndpointId Channel::AttachMessagePipeEndpoint(
    scoped_refptr<MessagePipe> message_pipe, unsigned port) {
  MessageInTransit::EndpointId local_id;
  {
    base::AutoLock locker(lock_);

    while (next_local_id_ == MessageInTransit::kInvalidEndpointId ||
           local_id_to_endpoint_info_map_.find(next_local_id_) !=
               local_id_to_endpoint_info_map_.end())
      next_local_id_++;

    local_id = next_local_id_;
    next_local_id_++;

    // TODO(vtl): Use emplace when we move to C++11 unordered_maps. (It'll avoid
    // some expensive reference count increment/decrements.) Once this is done,
    // we should be able to delete |EndpointInfo|'s default constructor.
    local_id_to_endpoint_info_map_[local_id] = EndpointInfo(message_pipe, port);
  }

  message_pipe->Attach(port, scoped_refptr<Channel>(this), local_id);
  return local_id;
}

void Channel::RunMessagePipeEndpoint(MessageInTransit::EndpointId local_id,
                                     MessageInTransit::EndpointId remote_id) {
  EndpointInfo endpoint_info;
  {
    base::AutoLock locker(lock_);

    IdToEndpointInfoMap::const_iterator it =
        local_id_to_endpoint_info_map_.find(local_id);
    CHECK(it != local_id_to_endpoint_info_map_.end());
    endpoint_info = it->second;
  }

  endpoint_info.message_pipe->Run(endpoint_info.port, remote_id);
}

bool Channel::WriteMessage(MessageInTransit* message) {
  base::AutoLock locker(lock_);
  if (!raw_channel_.get()) {
    // TODO(vtl): I think this is probably not an error condition, but I should
    // think about it (and the shutdown sequence) more carefully.
    LOG(WARNING) << "WriteMessage() after shutdown";
    return false;
  }

  return raw_channel_->WriteMessage(message);
}

void Channel::DetachMessagePipeEndpoint(MessageInTransit::EndpointId local_id) {
  DCHECK_NE(local_id, MessageInTransit::kInvalidEndpointId);

  base::AutoLock locker_(lock_);
  local_id_to_endpoint_info_map_.erase(local_id);
}

Channel::~Channel() {
  // The channel should have been shut down first.
  DCHECK(!raw_channel_.get());

  DLOG_IF(WARNING, !local_id_to_endpoint_info_map_.empty())
      << "Destroying Channel with " << local_id_to_endpoint_info_map_.size()
      << " endpoints still present";
}

void Channel::OnReadMessage(const MessageInTransit& message) {
  switch (message.type()) {
    case MessageInTransit::kTypeMessagePipeEndpoint:
    case MessageInTransit::kTypeMessagePipe:
      OnReadMessageForDownstream(message);
      break;
    case MessageInTransit::kTypeChannel:
      OnReadMessageForChannel(message);
      break;
    default:
      HandleRemoteError(base::StringPrintf(
          "Received message of invalid type %u",
          static_cast<unsigned>(message.type())));
      break;
  }
}

void Channel::OnFatalError(FatalError fatal_error) {
  // TODO(vtl): IMPORTANT. Notify all our endpoints that they're dead.
  NOTIMPLEMENTED();
}

void Channel::OnReadMessageForDownstream(const MessageInTransit& message) {
  DCHECK(message.type() == MessageInTransit::kTypeMessagePipeEndpoint ||
         message.type() == MessageInTransit::kTypeMessagePipe);

  MessageInTransit::EndpointId local_id = message.destination_id();
  if (local_id == MessageInTransit::kInvalidEndpointId) {
    HandleRemoteError("Received message with no destination ID");
    return;
  }

  EndpointInfo endpoint_info;
  {
    base::AutoLock locker(lock_);

    // Since we own |raw_channel_|, and this method and |Shutdown()| should only
    // be called from the creation thread, |raw_channel_| should never be null
    // here.
    DCHECK(raw_channel_.get());

    IdToEndpointInfoMap::const_iterator it =
        local_id_to_endpoint_info_map_.find(local_id);
    if (it == local_id_to_endpoint_info_map_.end()) {
      HandleRemoteError(base::StringPrintf(
          "Received a message for nonexistent local destination ID %u",
          static_cast<unsigned>(local_id)));
      // This is strongly indicative of some problem. However, it's not a fatal
      // error, since it may indicate a bug (or hostile) remote process. Don't
      // die even for Debug builds, since handling this properly needs to be
      // tested (TODO(vtl)).
      DLOG(ERROR) << "This should not happen under normal operation.";
      return;
    }
    endpoint_info = it->second;
  }

  // We need to duplicate the message, because |EnqueueMessage()| will take
  // ownership of it.
  // TODO(vtl): Need to enforce limits on message size and handle count.
  MessageInTransit* own_message = MessageInTransit::Create(
      message.type(), message.subtype(), message.data(), message.data_size(),
      message.num_handles());
  std::vector<Dispatcher*> dispatchers(message.num_handles());
  // TODO(vtl): Create dispatchers for handles.
  // TODO(vtl): It's bad that the current API will create equivalent dispatchers
  // for the freshly-created ones, which is totally redundant. Make a version of
  // |EnqueueMessage()| that passes ownership.
  if (endpoint_info.message_pipe->EnqueueMessage(
          MessagePipe::GetPeerPort(endpoint_info.port), own_message,
          message.num_handles() ? &dispatchers : NULL) != MOJO_RESULT_OK) {
    HandleLocalError(base::StringPrintf(
        "Failed to enqueue message to local destination ID %u",
        static_cast<unsigned>(local_id)));
    return;
  }
}

void Channel::OnReadMessageForChannel(const MessageInTransit& message) {
  // TODO(vtl): Currently no channel-only messages yet.
  HandleRemoteError("Received invalid channel message");
  NOTREACHED();
}

void Channel::HandleRemoteError(const base::StringPiece& error_message) {
  // TODO(vtl): Is this how we really want to handle this?
  LOG(WARNING) << error_message;
}

void Channel::HandleLocalError(const base::StringPiece& error_message) {
  // TODO(vtl): Is this how we really want to handle this?
  LOG(FATAL) << error_message;
}

}  // namespace system
}  // namespace mojo
