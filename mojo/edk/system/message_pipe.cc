// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/message_pipe.h"

#include "base/logging.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/channel_endpoint.h"
#include "mojo/edk/system/channel_endpoint_id.h"
#include "mojo/edk/system/endpoint_relayer.h"
#include "mojo/edk/system/incoming_endpoint.h"
#include "mojo/edk/system/local_message_pipe_endpoint.h"
#include "mojo/edk/system/message_in_transit.h"
#include "mojo/edk/system/message_pipe_dispatcher.h"
#include "mojo/edk/system/message_pipe_endpoint.h"
#include "mojo/edk/system/proxy_message_pipe_endpoint.h"

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
  DCHECK(!*channel_endpoint);  // Not technically wrong, but unlikely.
  MessagePipe* message_pipe = new MessagePipe();
  message_pipe->endpoints_[0].reset(new LocalMessagePipeEndpoint());
  *channel_endpoint = new ChannelEndpoint(message_pipe, 1);
  message_pipe->endpoints_[1].reset(
      new ProxyMessagePipeEndpoint(channel_endpoint->get()));
  return message_pipe;
}

// static
MessagePipe* MessagePipe::CreateLocalProxyFromExisting(
    MessageInTransitQueue* message_queue,
    ChannelEndpoint* channel_endpoint) {
  DCHECK(message_queue);
  MessagePipe* message_pipe = new MessagePipe();
  message_pipe->endpoints_[0].reset(
      new LocalMessagePipeEndpoint(message_queue));
  if (channel_endpoint) {
    bool attached_to_channel = channel_endpoint->ReplaceClient(message_pipe, 1);
    message_pipe->endpoints_[1].reset(
        new ProxyMessagePipeEndpoint(channel_endpoint));
    if (!attached_to_channel)
      message_pipe->OnDetachFromChannel(1);
  } else {
    // This means that the proxy side was already closed; we only need to inform
    // the local side of this.
    // TODO(vtl): This is safe to do without locking (but perhaps slightly
    // dubious), since no other thread has access to |message_pipe| yet.
    message_pipe->endpoints_[0]->OnPeerClose();
  }
  return message_pipe;
}

// static
MessagePipe* MessagePipe::CreateProxyLocal(
    scoped_refptr<ChannelEndpoint>* channel_endpoint) {
  DCHECK(!*channel_endpoint);  // Not technically wrong, but unlikely.
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

// static
bool MessagePipe::Deserialize(Channel* channel,
                              const void* source,
                              size_t size,
                              scoped_refptr<MessagePipe>* message_pipe,
                              unsigned* port) {
  DCHECK(!*message_pipe);  // Not technically wrong, but unlikely.

  if (size != channel->GetSerializedEndpointSize()) {
    LOG(ERROR) << "Invalid serialized message pipe";
    return false;
  }

  scoped_refptr<IncomingEndpoint> incoming_endpoint =
      channel->DeserializeEndpoint(source);
  if (!incoming_endpoint)
    return false;

  *message_pipe = incoming_endpoint->ConvertToMessagePipe();
  DCHECK(*message_pipe);
  *port = 0;
  return true;
}

MessagePipeEndpoint::Type MessagePipe::GetType(unsigned port) {
  DCHECK(port == 0 || port == 1);
  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port]);

  return endpoints_[port]->GetType();
}

void MessagePipe::CancelAllAwakables(unsigned port) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port]);
  endpoints_[port]->CancelAllAwakables();
}

void MessagePipe::Close(unsigned port) {
  DCHECK(port == 0 || port == 1);

  unsigned peer_port = GetPeerPort(port);

  base::AutoLock locker(lock_);
  // The endpoint's |OnPeerClose()| may have been called first and returned
  // false, which would have resulted in its destruction.
  if (!endpoints_[port])
    return;

  endpoints_[port]->Close();
  if (endpoints_[peer_port]) {
    if (!endpoints_[peer_port]->OnPeerClose())
      endpoints_[peer_port].reset();
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

  base::AutoLock locker(lock_);
  return EnqueueMessageNoLock(
      GetPeerPort(port),
      make_scoped_ptr(new MessageInTransit(
          MessageInTransit::kTypeEndpoint,
          MessageInTransit::kSubtypeEndpointData, num_bytes, bytes)),
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

  return endpoints_[port]->ReadMessage(bytes, num_bytes, dispatchers,
                                       num_dispatchers, flags);
}

HandleSignalsState MessagePipe::GetHandleSignalsState(unsigned port) const {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(const_cast<base::Lock&>(lock_));
  DCHECK(endpoints_[port]);

  return endpoints_[port]->GetHandleSignalsState();
}

MojoResult MessagePipe::AddAwakable(unsigned port,
                                    Awakable* awakable,
                                    MojoHandleSignals signals,
                                    uint32_t context,
                                    HandleSignalsState* signals_state) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port]);

  return endpoints_[port]->AddAwakable(awakable, signals, context,
                                       signals_state);
}

void MessagePipe::RemoveAwakable(unsigned port,
                                 Awakable* awakable,
                                 HandleSignalsState* signals_state) {
  DCHECK(port == 0 || port == 1);

  base::AutoLock locker(lock_);
  DCHECK(endpoints_[port]);

  endpoints_[port]->RemoveAwakable(awakable, signals_state);
}

void MessagePipe::StartSerialize(unsigned /*port*/,
                                 Channel* channel,
                                 size_t* max_size,
                                 size_t* max_platform_handles) {
  *max_size = channel->GetSerializedEndpointSize();
  *max_platform_handles = 0;
}

bool MessagePipe::EndSerialize(
    unsigned port,
    Channel* channel,
    void* destination,
    size_t* actual_size,
    embedder::PlatformHandleVector* /*platform_handles*/) {
  DCHECK(port == 0 || port == 1);

  scoped_refptr<ChannelEndpoint> channel_endpoint;
  {
    base::AutoLock locker(lock_);
    DCHECK(endpoints_[port]);

    // The port being serialized must be local.
    DCHECK_EQ(endpoints_[port]->GetType(), MessagePipeEndpoint::kTypeLocal);

    // There are three possibilities for the peer port (below). In all cases, we
    // pass the contents of |port|'s message queue to the channel, and it'll
    // (presumably) make a |ChannelEndpoint| from it.
    //
    //  1. The peer port is (known to be) closed.
    //
    //     There's no reason for us to continue to exist and no need for the
    //     channel to give us the |ChannelEndpoint|. It only remains for us to
    //     "close" |port|'s |LocalMessagePipeEndpoint| and prepare for
    //     destruction.
    //
    //  2. The peer port is local (the typical case).
    //
    //     The channel gives us back a |ChannelEndpoint|, which we hook up to a
    //     |ProxyMessagePipeEndpoint| to replace |port|'s
    //     |LocalMessagePipeEndpoint|. We continue to exist, since the peer
    //     port's message pipe dispatcher will continue to hold a reference to
    //     us.
    //
    //  3. The peer port is remote.
    //
    //     We also pass its |ChannelEndpoint| to the channel, which then decides
    //     what to do. We have no reason to continue to exist.
    //
    // TODO(vtl): Factor some of this out to |ChannelEndpoint| (or |Channel|).

    // The replacement for |endpoints_[port]|, if any.
    MessagePipeEndpoint* replacement_endpoint = nullptr;

    unsigned peer_port = GetPeerPort(port);
    if (!endpoints_[peer_port]) {  // Case 1.
      channel_endpoint = new ChannelEndpoint(
          nullptr, 0, static_cast<LocalMessagePipeEndpoint*>(
                          endpoints_[port].get())->message_queue());
    } else if (endpoints_[peer_port]->GetType() ==
               MessagePipeEndpoint::kTypeLocal) {  // Case 2.
      channel_endpoint = new ChannelEndpoint(
          this, port, static_cast<LocalMessagePipeEndpoint*>(
                          endpoints_[port].get())->message_queue());
      replacement_endpoint =
          new ProxyMessagePipeEndpoint(channel_endpoint.get());
    } else {  // Case 3.
      DLOG(WARNING) << "Direct message pipe passing across multiple channels "
                       "not yet implemented; will proxy";

      // Create an |EndpointRelayer| to replace ourselves (rather than having a
      // |MessagePipe| object that exists solely to relay messages between two
      // |ChannelEndpoint|s, owned by the |Channel| through them.
      //
      // This reduces overhead somewhat, and more importantly restores some
      // invariants, e.g., that |MessagePipe|s are owned by dispatchers.
      //
      // TODO(vtl): If we get the |Channel| to own/track the relayer directly,
      // then possibly we could make |ChannelEndpoint|'s |client_| pointer a raw
      // pointer (and not have the |Channel| owning the relayer via its
      // |ChannelEndpoint|s.
      //
      // TODO(vtl): This is not obviously the right place for (all of) this
      // logic, nor is it obviously factored correctly.

      DCHECK_EQ(endpoints_[peer_port]->GetType(),
                MessagePipeEndpoint::kTypeProxy);
      ProxyMessagePipeEndpoint* peer_endpoint =
          static_cast<ProxyMessagePipeEndpoint*>(endpoints_[peer_port].get());
      scoped_refptr<ChannelEndpoint> peer_channel_endpoint =
          peer_endpoint->ReleaseChannelEndpoint();

      scoped_refptr<EndpointRelayer> relayer(new EndpointRelayer());
      // We'll assign our peer port's endpoint to the relayer's port 1, and this
      // port's endpoint to the relayer's port 0.
      channel_endpoint = new ChannelEndpoint(
          relayer.get(), 0, static_cast<LocalMessagePipeEndpoint*>(
                                endpoints_[port].get())->message_queue());
      relayer->Init(channel_endpoint.get(), peer_channel_endpoint.get());
      peer_channel_endpoint->ReplaceClient(relayer.get(), 1);

      // No need to call |Close()| after |ReleaseChannelEndpoint()|.
      endpoints_[peer_port].reset();
    }

    endpoints_[port]->Close();
    endpoints_[port].reset(replacement_endpoint);
  }

  // TODO(vtl): More/most of the above should be moved into (some variant of)
  // |Channel::SerializeEndpoint()|.
  channel->SerializeEndpoint(channel_endpoint, destination);
  *actual_size = channel->GetSerializedEndpointSize();
  return true;
}

bool MessagePipe::OnReadMessage(unsigned port, MessageInTransit* message) {
  base::AutoLock locker(lock_);

  if (!endpoints_[port]) {
    // This will happen only on the rare occasion that the call to
    // |OnReadMessage()| is racing with us calling
    // |ChannelEndpoint::ReplaceClient()|, in which case we reject the message,
    // and the |ChannelEndpoint| can retry (calling the new client's
    // |OnReadMessage()|).
    return false;
  }

  // This is called when the |ChannelEndpoint| for the
  // |ProxyMessagePipeEndpoint| |port| receives a message (from the |Channel|).
  // We need to pass this message on to its peer port (typically a
  // |LocalMessagePipeEndpoint|).
  MojoResult result = EnqueueMessageNoLock(GetPeerPort(port),
                                           make_scoped_ptr(message), nullptr);
  DLOG_IF(WARNING, result != MOJO_RESULT_OK)
      << "EnqueueMessageNoLock() failed (result  = " << result << ")";
  return true;
}

void MessagePipe::OnDetachFromChannel(unsigned port) {
  Close(port);
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

MojoResult MessagePipe::EnqueueMessageNoLock(
    unsigned port,
    scoped_ptr<MessageInTransit> message,
    std::vector<DispatcherTransport>* transports) {
  DCHECK(port == 0 || port == 1);
  DCHECK(message);

  DCHECK_EQ(message->type(), MessageInTransit::kTypeEndpoint);
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
      dispatchers->push_back(nullptr);
    }
  }
  message->SetDispatchers(dispatchers.Pass());
  return MOJO_RESULT_OK;
}

}  // namespace system
}  // namespace mojo
