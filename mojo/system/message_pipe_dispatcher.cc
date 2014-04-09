// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/message_pipe_dispatcher.h"

#include "base/logging.h"
#include "mojo/system/channel.h"
#include "mojo/system/constants.h"
#include "mojo/system/local_message_pipe_endpoint.h"
#include "mojo/system/memory.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/message_pipe.h"
#include "mojo/system/proxy_message_pipe_endpoint.h"

namespace mojo {
namespace system {

namespace {

const unsigned kInvalidPort = static_cast<unsigned>(-1);

struct SerializedMessagePipeDispatcher {
  MessageInTransit::EndpointId endpoint_id;
};

}  // namespace

// MessagePipeDispatcher -------------------------------------------------------

MessagePipeDispatcher::MessagePipeDispatcher()
    : port_(kInvalidPort) {
}

void MessagePipeDispatcher::Init(scoped_refptr<MessagePipe> message_pipe,
                                 unsigned port) {
  DCHECK(message_pipe.get());
  DCHECK(port == 0 || port == 1);

  message_pipe_ = message_pipe;
  port_ = port;
}

Dispatcher::Type MessagePipeDispatcher::GetType() const {
  return kTypeMessagePipe;
}

// static
std::pair<scoped_refptr<MessagePipeDispatcher>, scoped_refptr<MessagePipe> >
MessagePipeDispatcher::CreateRemoteMessagePipe() {
  scoped_refptr<MessagePipe> message_pipe(
      new MessagePipe(
          scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
          scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  scoped_refptr<MessagePipeDispatcher> dispatcher(new MessagePipeDispatcher());
  dispatcher->Init(message_pipe, 0);

  return std::make_pair(dispatcher, message_pipe);
}

// static
scoped_refptr<MessagePipeDispatcher> MessagePipeDispatcher::Deserialize(
    Channel* channel,
    const void* source,
    size_t size) {
  if (size != sizeof(SerializedMessagePipeDispatcher)) {
    LOG(ERROR) << "Invalid serialized message pipe dispatcher";
    return scoped_refptr<MessagePipeDispatcher>();
  }

  std::pair<scoped_refptr<MessagePipeDispatcher>, scoped_refptr<MessagePipe> >
      remote_message_pipe = CreateRemoteMessagePipe();

  MessageInTransit::EndpointId remote_id =
      static_cast<const SerializedMessagePipeDispatcher*>(source)->endpoint_id;
  MessageInTransit::EndpointId local_id =
      channel->AttachMessagePipeEndpoint(remote_message_pipe.second, 1);
  DVLOG(2) << "Deserializing message pipe dispatcher (remote ID = "
           << remote_id << ", new local ID = " << local_id << ")";

  channel->RunMessagePipeEndpoint(local_id, remote_id);
  // TODO(vtl): FIXME -- Need some error handling here.
  channel->RunRemoteMessagePipeEndpoint(local_id, remote_id);
  return remote_message_pipe.first;
}

MessagePipeDispatcher::~MessagePipeDispatcher() {
  // |Close()|/|CloseImplNoLock()| should have taken care of the pipe.
  DCHECK(!message_pipe_.get());
}

MessagePipe* MessagePipeDispatcher::GetMessagePipeNoLock() const {
  lock().AssertAcquired();
  return message_pipe_.get();
}

unsigned MessagePipeDispatcher::GetPortNoLock() const {
  lock().AssertAcquired();
  return port_;
}

void MessagePipeDispatcher::CancelAllWaitersNoLock() {
  lock().AssertAcquired();
  message_pipe_->CancelAllWaiters(port_);
}

void MessagePipeDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  message_pipe_->Close(port_);
  message_pipe_ = NULL;
  port_ = kInvalidPort;
}

scoped_refptr<Dispatcher>
MessagePipeDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();

  scoped_refptr<MessagePipeDispatcher> rv = new MessagePipeDispatcher();
  rv->Init(message_pipe_, port_);
  message_pipe_ = NULL;
  port_ = kInvalidPort;
  return scoped_refptr<Dispatcher>(rv.get());
}

MojoResult MessagePipeDispatcher::WriteMessageImplNoLock(
    const void* bytes,
    uint32_t num_bytes,
    std::vector<DispatcherTransport>* transports,
    MojoWriteMessageFlags flags) {
  DCHECK(!transports || (transports->size() > 0 &&
                         transports->size() <= kMaxMessageNumHandles));

  lock().AssertAcquired();

  if (!VerifyUserPointer<void>(bytes, num_bytes))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (num_bytes > kMaxMessageNumBytes)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  return message_pipe_->WriteMessage(port_, bytes, num_bytes, transports,
                                     flags);
}

MojoResult MessagePipeDispatcher::ReadMessageImplNoLock(
    void* bytes,
    uint32_t* num_bytes,
    std::vector<scoped_refptr<Dispatcher> >* dispatchers,
    uint32_t* num_dispatchers,
    MojoReadMessageFlags flags) {
  lock().AssertAcquired();

  if (num_bytes) {
    if (!VerifyUserPointer<uint32_t>(num_bytes, 1))
      return MOJO_RESULT_INVALID_ARGUMENT;
    if (!VerifyUserPointer<void>(bytes, *num_bytes))
      return MOJO_RESULT_INVALID_ARGUMENT;
  }

  return message_pipe_->ReadMessage(port_, bytes, num_bytes, dispatchers,
                                    num_dispatchers, flags);
}

MojoResult MessagePipeDispatcher::AddWaiterImplNoLock(Waiter* waiter,
                                                      MojoWaitFlags flags,
                                                      MojoResult wake_result) {
  lock().AssertAcquired();
  return message_pipe_->AddWaiter(port_, waiter, flags, wake_result);
}

void MessagePipeDispatcher::RemoveWaiterImplNoLock(Waiter* waiter) {
  lock().AssertAcquired();
  message_pipe_->RemoveWaiter(port_, waiter);
}

void MessagePipeDispatcher::StartSerializeImplNoLock(
    Channel* /*channel*/,
    size_t* max_size,
    size_t* max_platform_handles) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.
  *max_size = sizeof(SerializedMessagePipeDispatcher);
  *max_platform_handles = 0;
}

bool MessagePipeDispatcher::EndSerializeAndCloseImplNoLock(
    Channel* channel,
    void* destination,
    size_t* actual_size,
    std::vector<embedder::PlatformHandle>* platform_handles) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.

  // Convert the local endpoint to a proxy endpoint (moving the message queue).
  message_pipe_->ConvertLocalToProxy(port_);

  // Attach the new proxy endpoint to the channel.
  MessageInTransit::EndpointId endpoint_id =
      channel->AttachMessagePipeEndpoint(message_pipe_, port_);
  DCHECK_NE(endpoint_id, MessageInTransit::kInvalidEndpointId);

  DVLOG(2) << "Serializing message pipe dispatcher (local ID = " << endpoint_id
           << ")";

  // We now have a local ID. Before we can run the proxy endpoint, we need to
  // get an ack back from the other side with the remote ID.
  static_cast<SerializedMessagePipeDispatcher*>(destination)->endpoint_id =
      endpoint_id;

  message_pipe_ = NULL;
  port_ = kInvalidPort;

  *actual_size = sizeof(SerializedMessagePipeDispatcher);
  return true;
}

// MessagePipeDispatcherTransport ----------------------------------------------

MessagePipeDispatcherTransport::MessagePipeDispatcherTransport(
    DispatcherTransport transport) : DispatcherTransport(transport) {
  DCHECK_EQ(message_pipe_dispatcher()->GetType(), Dispatcher::kTypeMessagePipe);
}

}  // namespace system
}  // namespace mojo
