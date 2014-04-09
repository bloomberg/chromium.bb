// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/channel.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "mojo/system/message_pipe_endpoint.h"

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

bool Channel::Init(scoped_ptr<RawChannel> raw_channel) {
  DCHECK(creation_thread_checker_.CalledOnValidThread());
  DCHECK(raw_channel);

  // No need to take |lock_|, since this must be called before this object
  // becomes thread-safe.
  DCHECK(!raw_channel_);
  raw_channel_ = raw_channel.Pass();

  if (!raw_channel_->Init(this)) {
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

  // This should not occur, but it probably mostly results in leaking;
  // (Explicitly clearing the |local_id_to_endpoint_info_map_| would likely put
  // things in an inconsistent state, which is worse. Note that if the map is
  // nonempty, we probably won't be destroyed, since the endpoints have a
  // reference to us.)
  LOG_IF(ERROR, local_id_to_endpoint_info_map_.empty())
      << "Channel shutting down with endpoints still attached";
  // TODO(vtl): This currently blows up, but the fix will be nontrivial.
  // crbug.com/360081
  //DCHECK(local_id_to_endpoint_info_map_.empty());
}

MessageInTransit::EndpointId Channel::AttachMessagePipeEndpoint(
    scoped_refptr<MessagePipe> message_pipe, unsigned port) {
  DCHECK(port == 0 || port == 1);
  // Note: This assertion must *not* be done under |lock_|.
  DCHECK_EQ(message_pipe->GetType(port), MessagePipeEndpoint::kTypeProxy);

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
    // TODO(vtl): FIXME -- This check is wrong if this is in response to a
    // |kSubtypeChannelRunMessagePipeEndpoint| message. We should report error.
    CHECK(it != local_id_to_endpoint_info_map_.end());
    endpoint_info = it->second;
  }

  // TODO(vtl): FIXME -- We need to handle the case that message pipe is already
  // running when we're here due to |kSubtypeChannelRunMessagePipeEndpoint|).
  endpoint_info.message_pipe->Run(endpoint_info.port, remote_id);
}

void Channel::RunRemoteMessagePipeEndpoint(
    MessageInTransit::EndpointId local_id,
    MessageInTransit::EndpointId remote_id) {
  base::AutoLock locker(lock_);

  DCHECK(local_id_to_endpoint_info_map_.find(local_id) !=
             local_id_to_endpoint_info_map_.end());

  scoped_ptr<MessageInTransit> message(new MessageInTransit(
      MessageInTransit::kTypeChannel,
      MessageInTransit::kSubtypeChannelRunMessagePipeEndpoint,
      0, 0, NULL));
  message->set_source_id(local_id);
  message->set_destination_id(remote_id);
  if (!raw_channel_->WriteMessage(message.Pass())) {
    // TODO(vtl): FIXME -- I guess we should report the error back somehow so
    // that the dispatcher can be closed?
    CHECK(false) << "Not yet handled";
  }
}

bool Channel::WriteMessage(scoped_ptr<MessageInTransit> message) {
  base::AutoLock locker(lock_);
  if (!raw_channel_.get()) {
    // TODO(vtl): I think this is probably not an error condition, but I should
    // think about it (and the shutdown sequence) more carefully.
    LOG(WARNING) << "WriteMessage() after shutdown";
    return false;
  }

  return raw_channel_->WriteMessage(message.Pass());
}

bool Channel::IsWriteBufferEmpty() {
  base::AutoLock locker(lock_);
  DCHECK(raw_channel_.get());
  return raw_channel_->IsWriteBufferEmpty();
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

void Channel::OnReadMessage(const MessageInTransit::View& message_view) {
  // Note: |ValidateReadMessage()| will call |HandleRemoteError()| if necessary.
  if (!ValidateReadMessage(message_view))
    return;

  switch (message_view.type()) {
    case MessageInTransit::kTypeMessagePipeEndpoint:
    case MessageInTransit::kTypeMessagePipe:
      OnReadMessageForDownstream(message_view);
      break;
    case MessageInTransit::kTypeChannel:
      OnReadMessageForChannel(message_view);
      break;
    default:
      HandleRemoteError(base::StringPrintf(
          "Received message of invalid type %u",
          static_cast<unsigned>(message_view.type())));
      break;
  }
}

void Channel::OnFatalError(FatalError fatal_error) {
  // TODO(vtl): IMPORTANT. Notify all our endpoints that they're dead.
  NOTIMPLEMENTED();
}

bool Channel::ValidateReadMessage(const MessageInTransit::View& message_view) {
  const char* error_message = NULL;
  if (!message_view.IsValid(&error_message)) {
    DCHECK(error_message);
    HandleRemoteError(error_message);
    return false;
  }

  return true;
}

void Channel::OnReadMessageForDownstream(
    const MessageInTransit::View& message_view) {
  DCHECK(message_view.type() == MessageInTransit::kTypeMessagePipeEndpoint ||
         message_view.type() == MessageInTransit::kTypeMessagePipe);

  MessageInTransit::EndpointId local_id = message_view.destination_id();
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
  scoped_ptr<MessageInTransit> message(new MessageInTransit(message_view));
  message->DeserializeDispatchers(this);
  MojoResult result = endpoint_info.message_pipe->EnqueueMessage(
      MessagePipe::GetPeerPort(endpoint_info.port), message.Pass(), NULL);
  if (result != MOJO_RESULT_OK) {
    HandleLocalError(base::StringPrintf(
        "Failed to enqueue message to local destination ID %u (result %d)",
        static_cast<unsigned>(local_id), static_cast<int>(result)));
    return;
  }
}

void Channel::OnReadMessageForChannel(
    const MessageInTransit::View& message_view) {
  DCHECK_EQ(message_view.type(), MessageInTransit::kTypeChannel);

  switch (message_view.subtype()) {
    case MessageInTransit::kSubtypeChannelRunMessagePipeEndpoint:
      // TODO(vtl): FIXME -- Error handling (also validation of
      // source/destination IDs).
      DVLOG(2) << "Handling channel message to run message pipe (local ID = "
               << message_view.destination_id() << ", remote ID = "
               << message_view.source_id() << ")";
      RunMessagePipeEndpoint(message_view.destination_id(),
                             message_view.source_id());
      break;
    default:
      HandleRemoteError("Received invalid channel message");
      NOTREACHED();
      break;
  }
}

void Channel::HandleRemoteError(const base::StringPiece& error_message) {
  // TODO(vtl): Is this how we really want to handle this? Probably we want to
  // terminate the connection, since it's spewing invalid stuff.
  LOG(WARNING) << error_message;
}

void Channel::HandleLocalError(const base::StringPiece& error_message) {
  // TODO(vtl): Is this how we really want to handle this?
  LOG(WARNING) << error_message;
}

}  // namespace system
}  // namespace mojo
