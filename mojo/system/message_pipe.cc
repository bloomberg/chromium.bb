// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/message_pipe.h"

#include "base/logging.h"
#include "mojo/system/channel.h"
#include "mojo/system/local_message_pipe_endpoint.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/message_pipe_dispatcher.h"
#include "mojo/system/message_pipe_endpoint.h"
#include "mojo/system/proxy_message_pipe_endpoint.h"

namespace mojo {
namespace system {

MessagePipe::MessagePipe(scoped_ptr<MessagePipeEndpoint> endpoint0,
                         scoped_ptr<MessagePipeEndpoint> endpoint1) {
  endpoints_[0].reset(endpoint0.release());
  endpoints_[1].reset(endpoint1.release());
}

MessagePipe::MessagePipe() {
  endpoints_[0].reset(new LocalMessagePipeEndpoint());
  endpoints_[1].reset(new LocalMessagePipeEndpoint());
}

// static
unsigned MessagePipe::GetPeerPort(unsigned port) {
  DCHECK(port == 0 || port == 1);
  return port ^ 1;
}

void MessagePipe::CancelAllWaiters(unsigned port) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port].get());
  endpoints_[port]->CancelAllWaiters();
}

void MessagePipe::Close(unsigned port) {
  DCHECK(port == 0 || port == 1);

  unsigned destination_port = GetPeerPort(port);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port].get());

  endpoints_[port]->Close();
  if (endpoints_[destination_port].get())
    endpoints_[destination_port]->OnPeerClose();
  endpoints_[port].reset();
}

// TODO(vtl): Support sending handles.
// TODO(vtl): Handle flags.
MojoResult MessagePipe::WriteMessage(
    unsigned port,
    const void* bytes,
    uint32_t num_bytes,
    std::vector<DispatcherTransport>* transports,
    MojoWriteMessageFlags flags) {
  DCHECK(port == 0 || port == 1);
  uint32_t num_handles =
      transports ? static_cast<uint32_t>(transports->size()) : 0;
  return EnqueueMessage(
      GetPeerPort(port),
      make_scoped_ptr(new MessageInTransit(
          MessageInTransit::OWNED_BUFFER,
          MessageInTransit::kTypeMessagePipeEndpoint,
          MessageInTransit::kSubtypeMessagePipeEndpointData,
          num_bytes, num_handles, bytes)),
      transports);
}

MojoResult MessagePipe::ReadMessage(
    unsigned port,
    void* bytes,
    uint32_t* num_bytes,
    std::vector<scoped_refptr<Dispatcher> >* dispatchers,
    uint32_t* num_dispatchers,
    MojoReadMessageFlags flags) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port].get());

  return endpoints_[port]->ReadMessage(bytes, num_bytes, dispatchers,
                                       num_dispatchers, flags);
}

MojoResult MessagePipe::AddWaiter(unsigned port,
                                  Waiter* waiter,
                                  MojoWaitFlags flags,
                                  MojoResult wake_result) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port].get());

  return endpoints_[port]->AddWaiter(waiter, flags, wake_result);
}

void MessagePipe::RemoveWaiter(unsigned port, Waiter* waiter) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port].get());

  endpoints_[port]->RemoveWaiter(waiter);
}

MojoResult MessagePipe::EnqueueMessage(
    unsigned port,
    scoped_ptr<MessageInTransit> message,
    std::vector<DispatcherTransport>* transports) {
  DCHECK(port == 0 || port == 1);
  DCHECK(message.get());
  DCHECK((!transports && message->num_handles() == 0) ||
         (transports && transports->size() > 0 &&
              message->num_handles() == transports->size()));

  if (message->type() == MessageInTransit::kTypeMessagePipe) {
    DCHECK(!transports);
    return HandleControlMessage(port, message.Pass());
  }

  DCHECK_EQ(message->type(), MessageInTransit::kTypeMessagePipeEndpoint);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[GetPeerPort(port)].get());

  // The destination port need not be open, unlike the source port.
  if (!endpoints_[port].get())
    return MOJO_RESULT_FAILED_PRECONDITION;

  if (transports) {
    // You're not allowed to send either handle to a message pipe over the
    // message pipe, so check for this. (The case of trying to write a handle to
    // itself is taken care of by |CoreImpl|. That case kind of makes sense, but
    // leads to complications if, e.g., both sides try to do the same thing with
    // their respective handles simultaneously. The other case, of trying to
    // write the peer handle to a handle, doesn't make sense -- since no handle
    // will be available to read the message from.)
    for (size_t i = 0; i < transports->size(); i++) {
      if (!(*transports)[i].is_valid())
        continue;
      if ((*transports)[i].GetType() == Dispatcher::kTypeMessagePipe) {
        MessagePipeDispatcherTransport mp_transport((*transports)[i]);
        if (mp_transport.GetMessagePipe() == this) {
          // The other case should have been disallowed by |CoreImpl|. (Note:
          // |port| is the peer port of the handle given to |WriteMessage()|.)
          DCHECK_EQ(mp_transport.GetPort(), port);
          return MOJO_RESULT_INVALID_ARGUMENT;
        }
      }
    }
  }

  return endpoints_[port]->EnqueueMessage(message.Pass(), transports);
}

void MessagePipe::Attach(unsigned port,
                         scoped_refptr<Channel> channel,
                         MessageInTransit::EndpointId local_id) {
  DCHECK(port == 0 || port == 1);
  DCHECK(channel.get());
  DCHECK_NE(local_id, MessageInTransit::kInvalidEndpointId);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port].get());

  endpoints_[port]->Attach(channel, local_id);
}

void MessagePipe::Run(unsigned port, MessageInTransit::EndpointId remote_id) {
  DCHECK(port == 0 || port == 1);
  DCHECK_NE(remote_id, MessageInTransit::kInvalidEndpointId);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port].get());
  endpoints_[port]->Run(remote_id);
}

MessagePipe::~MessagePipe() {
  // Owned by the dispatchers. The owning dispatchers should only release us via
  // their |Close()| method, which should inform us of being closed via our
  // |Close()|. Thus these should already be null.
  DCHECK(!endpoints_[0].get());
  DCHECK(!endpoints_[1].get());
}

MojoResult MessagePipe::HandleControlMessage(
    unsigned port,
    scoped_ptr<MessageInTransit> message) {
  DCHECK(port == 0 || port == 1);
  DCHECK(message.get());
  DCHECK_EQ(message->type(), MessageInTransit::kTypeMessagePipe);

  switch (message->subtype()) {
    case MessageInTransit::kSubtypeMessagePipePeerClosed: {
      unsigned source_port = GetPeerPort(port);

      base::AutoLock locker(lock_);
      DCHECK(endpoints_[source_port].get());

      endpoints_[source_port]->Close();
      if (endpoints_[port].get())
        endpoints_[port]->OnPeerClose();

      endpoints_[source_port].reset();
      return MOJO_RESULT_OK;
    }
  }

  LOG(WARNING) << "Unrecognized MessagePipe control message subtype "
               << message->subtype();
  return MOJO_RESULT_UNKNOWN;
}

}  // namespace system
}  // namespace mojo
