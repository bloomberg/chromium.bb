// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/message_pipe.h"

#include "base/logging.h"
#include "mojo/system/channel_endpoint.h"
#include "mojo/system/local_message_pipe_endpoint.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/message_pipe_dispatcher.h"
#include "mojo/system/message_pipe_endpoint.h"
#include "mojo/system/proxy_message_pipe_endpoint.h"

namespace mojo {
namespace system {

// static
MessagePipe* MessagePipe::CreateLocalLocal() {
  MessagePipe* message_pipe = new MessagePipe();
  message_pipe->endpoints_[0].reset(new LocalMessagePipeEndpoint());
  message_pipe->endpoints_[1].reset(new LocalMessagePipeEndpoint());
  return message_pipe;
}

// static
MessagePipe* MessagePipe::CreateLocalProxy(
    scoped_refptr<ChannelEndpoint>* channel_endpoint) {
  DCHECK(!channel_endpoint->get());  // Not technically wrong, but unlikely.
  MessagePipe* message_pipe = new MessagePipe();
  message_pipe->endpoints_[0].reset(new LocalMessagePipeEndpoint());
  *channel_endpoint = new ChannelEndpoint(message_pipe, 1);
  message_pipe->endpoints_[1].reset(
      new ProxyMessagePipeEndpoint(channel_endpoint->get()));
  return message_pipe;
}

// static
MessagePipe* MessagePipe::CreateProxyLocal(
    scoped_refptr<ChannelEndpoint>* channel_endpoint) {
  DCHECK(!channel_endpoint->get());  // Not technically wrong, but unlikely.
  MessagePipe* message_pipe = new MessagePipe();
  *channel_endpoint = new ChannelEndpoint(message_pipe, 0);
  message_pipe->endpoints_[0].reset(
      new ProxyMessagePipeEndpoint(channel_endpoint->get()));
  message_pipe->endpoints_[1].reset(new LocalMessagePipeEndpoint());
  return message_pipe;
}

// static
unsigned MessagePipe::GetPeerPort(unsigned port) {
  DCHECK(port == 0 || port == 1);
  return port ^ 1;
}

MessagePipeEndpoint::Type MessagePipe::GetType(unsigned port) {
  DCHECK(port == 0 || port == 1);
  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port]);

  return endpoints_[port]->GetType();
}

void MessagePipe::CancelAllWaiters(unsigned port) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port]);
  endpoints_[port]->CancelAllWaiters();
}

void MessagePipe::Close(unsigned port) {
  DCHECK(port == 0 || port == 1);

  unsigned destination_port = GetPeerPort(port);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port]);

  endpoints_[port]->Close();
  if (endpoints_[destination_port]) {
    if (!endpoints_[destination_port]->OnPeerClose())
      endpoints_[destination_port].reset();
  }
  endpoints_[port].reset();
}

// TODO(vtl): Handle flags.
MojoResult MessagePipe::WriteMessage(
    unsigned port,
    UserPointer<const void> bytes,
    uint32_t num_bytes,
    std::vector<DispatcherTransport>* transports,
    MojoWriteMessageFlags flags) {
  DCHECK(port == 0 || port == 1);
  return EnqueueMessageInternal(
      GetPeerPort(port),
      make_scoped_ptr(new MessageInTransit(
          MessageInTransit::kTypeMessagePipeEndpoint,
          MessageInTransit::kSubtypeMessagePipeEndpointData,
          num_bytes,
          bytes)),
      transports);
}

MojoResult MessagePipe::ReadMessage(unsigned port,
                                    UserPointer<void> bytes,
                                    UserPointer<uint32_t> num_bytes,
                                    DispatcherVector* dispatchers,
                                    uint32_t* num_dispatchers,
                                    MojoReadMessageFlags flags) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port]);

  return endpoints_[port]->ReadMessage(
      bytes, num_bytes, dispatchers, num_dispatchers, flags);
}

HandleSignalsState MessagePipe::GetHandleSignalsState(unsigned port) const {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(const_cast<base::Lock&>(lock_));
  DCHECK(endpoints_[port]);

  return endpoints_[port]->GetHandleSignalsState();
}

MojoResult MessagePipe::AddWaiter(unsigned port,
                                  Waiter* waiter,
                                  MojoHandleSignals signals,
                                  uint32_t context,
                                  HandleSignalsState* signals_state) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port]);

  return endpoints_[port]->AddWaiter(waiter, signals, context, signals_state);
}

void MessagePipe::RemoveWaiter(unsigned port,
                               Waiter* waiter,
                               HandleSignalsState* signals_state) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port]);

  endpoints_[port]->RemoveWaiter(waiter, signals_state);
}

scoped_refptr<ChannelEndpoint> MessagePipe::ConvertLocalToProxy(unsigned port) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port]);
  DCHECK_EQ(endpoints_[port]->GetType(), MessagePipeEndpoint::kTypeLocal);

  // TODO(vtl): Allowing this case is a temporary hack. It'll set up a
  // |MessagePipe| with two proxy endpoints, which will then act as a proxy
  // (rather than trying to connect the two ends directly).
  DLOG_IF(WARNING,
          !!endpoints_[GetPeerPort(port)] &&
              endpoints_[GetPeerPort(port)]->GetType() !=
                  MessagePipeEndpoint::kTypeLocal)
      << "Direct message pipe passing across multiple channels not yet "
         "implemented; will proxy";

  scoped_ptr<MessagePipeEndpoint> old_endpoint(endpoints_[port].Pass());
  scoped_refptr<ChannelEndpoint> channel_endpoint(
      new ChannelEndpoint(this, port));
  endpoints_[port].reset(new ProxyMessagePipeEndpoint(channel_endpoint.get()));
  channel_endpoint->TakeMessages(static_cast<LocalMessagePipeEndpoint*>(
                                     old_endpoint.get())->message_queue());
  old_endpoint->Close();

  return channel_endpoint;
}

MojoResult MessagePipe::EnqueueMessage(unsigned port,
                                       scoped_ptr<MessageInTransit> message) {
  return EnqueueMessageInternal(port, message.Pass(), nullptr);
}

void MessagePipe::OnRemove(unsigned port) {
  unsigned destination_port = GetPeerPort(port);

  base::AutoLock locker(lock_);
  // A |OnPeerClose()| can come in first, before |OnRemove()| gets called.
  if (!endpoints_[port])
    return;

  if (endpoints_[destination_port]) {
    if (!endpoints_[destination_port]->OnPeerClose())
      endpoints_[destination_port].reset();
  }
  endpoints_[port].reset();
}

MessagePipe::MessagePipe() {
}

MessagePipe::~MessagePipe() {
  // Owned by the dispatchers. The owning dispatchers should only release us via
  // their |Close()| method, which should inform us of being closed via our
  // |Close()|. Thus these should already be null.
  DCHECK(!endpoints_[0]);
  DCHECK(!endpoints_[1]);
}

MojoResult MessagePipe::EnqueueMessageInternal(
    unsigned port,
    scoped_ptr<MessageInTransit> message,
    std::vector<DispatcherTransport>* transports) {
  DCHECK(port == 0 || port == 1);
  DCHECK(message);

  if (message->type() == MessageInTransit::kTypeMessagePipe) {
    DCHECK(!transports);
    return HandleControlMessage(port, message.Pass());
  }

  DCHECK_EQ(message->type(), MessageInTransit::kTypeMessagePipeEndpoint);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[GetPeerPort(port)]);

  // The destination port need not be open, unlike the source port.
  if (!endpoints_[port])
    return MOJO_RESULT_FAILED_PRECONDITION;

  if (transports) {
    MojoResult result = AttachTransportsNoLock(port, message.get(), transports);
    if (result != MOJO_RESULT_OK)
      return result;
  }

  // The endpoint's |EnqueueMessage()| may not report failure.
  endpoints_[port]->EnqueueMessage(message.Pass());
  return MOJO_RESULT_OK;
}

MojoResult MessagePipe::AttachTransportsNoLock(
    unsigned port,
    MessageInTransit* message,
    std::vector<DispatcherTransport>* transports) {
  DCHECK(!message->has_dispatchers());

  // You're not allowed to send either handle to a message pipe over the message
  // pipe, so check for this. (The case of trying to write a handle to itself is
  // taken care of by |Core|. That case kind of makes sense, but leads to
  // complications if, e.g., both sides try to do the same thing with their
  // respective handles simultaneously. The other case, of trying to write the
  // peer handle to a handle, doesn't make sense -- since no handle will be
  // available to read the message from.)
  for (size_t i = 0; i < transports->size(); i++) {
    if (!(*transports)[i].is_valid())
      continue;
    if ((*transports)[i].GetType() == Dispatcher::kTypeMessagePipe) {
      MessagePipeDispatcherTransport mp_transport((*transports)[i]);
      if (mp_transport.GetMessagePipe() == this) {
        // The other case should have been disallowed by |Core|. (Note: |port|
        // is the peer port of the handle given to |WriteMessage()|.)
        DCHECK_EQ(mp_transport.GetPort(), port);
        return MOJO_RESULT_INVALID_ARGUMENT;
      }
    }
  }

  // Clone the dispatchers and attach them to the message. (This must be done as
  // a separate loop, since we want to leave the dispatchers alone on failure.)
  scoped_ptr<DispatcherVector> dispatchers(new DispatcherVector());
  dispatchers->reserve(transports->size());
  for (size_t i = 0; i < transports->size(); i++) {
    if ((*transports)[i].is_valid()) {
      dispatchers->push_back(
          (*transports)[i].CreateEquivalentDispatcherAndClose());
    } else {
      LOG(WARNING) << "Enqueueing null dispatcher";
      dispatchers->push_back(scoped_refptr<Dispatcher>());
    }
  }
  message->SetDispatchers(dispatchers.Pass());
  return MOJO_RESULT_OK;
}

MojoResult MessagePipe::HandleControlMessage(
    unsigned /*port*/,
    scoped_ptr<MessageInTransit> message) {
  LOG(WARNING) << "Unrecognized MessagePipe control message subtype "
               << message->subtype();
  return MOJO_RESULT_UNKNOWN;
}

}  // namespace system
}  // namespace mojo
