// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_MESSAGE_PIPE_H_
#define MOJO_SYSTEM_MESSAGE_PIPE_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/handle_signals_state.h"
#include "mojo/system/memory.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/message_pipe_endpoint.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

class ChannelEndpoint;
class Waiter;

// |MessagePipe| is the secondary object implementing a message pipe (see the
// explanatory comment in core.cc). It is typically owned by the dispatcher(s)
// corresponding to the local endpoints. This class is thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT MessagePipe
    : public base::RefCountedThreadSafe<MessagePipe> {
 public:
  // Creates a |MessagePipe| with two new |LocalMessagePipeEndpoint|s.
  static MessagePipe* CreateLocalLocal();

  // Creates a |MessagePipe| with a |LocalMessagePipeEndpoint| on port 0 and a
  // |ProxyMessagePipeEndpoint| on port 1. |*channel_endpoint| is set to the
  // (newly-created) |ChannelEndpoint| for the latter.
  static MessagePipe* CreateLocalProxy(
      scoped_refptr<ChannelEndpoint>* channel_endpoint);

  // Creates a |MessagePipe| with a |ProxyMessagePipeEndpoint| on port 0 and a
  // |LocalMessagePipeEndpoint| on port 1. |*channel_endpoint| is set to the
  // (newly-created) |ChannelEndpoint| for the former.
  // Note: This is really only needed in tests (outside of tests, this
  // configuration arises from a local message pipe having its port 0
  // "converted" using |ConvertLocalToProxy()|).
  static MessagePipe* CreateProxyLocal(
      scoped_refptr<ChannelEndpoint>* channel_endpoint);

  // Gets the other port number (i.e., 0 -> 1, 1 -> 0).
  static unsigned GetPeerPort(unsigned port);

  // Gets the type of the endpoint (used for assertions, etc.).
  MessagePipeEndpoint::Type GetType(unsigned port);

  // These are called by the dispatcher to implement its methods of
  // corresponding names. In all cases, the port |port| must be open.
  void CancelAllWaiters(unsigned port);
  void Close(unsigned port);
  // Unlike |MessagePipeDispatcher::WriteMessage()|, this does not validate its
  // arguments.
  MojoResult WriteMessage(unsigned port,
                          UserPointer<const void> bytes,
                          uint32_t num_bytes,
                          std::vector<DispatcherTransport>* transports,
                          MojoWriteMessageFlags flags);
  MojoResult ReadMessage(unsigned port,
                         UserPointer<void> bytes,
                         UserPointer<uint32_t> num_bytes,
                         DispatcherVector* dispatchers,
                         uint32_t* num_dispatchers,
                         MojoReadMessageFlags flags);
  HandleSignalsState GetHandleSignalsState(unsigned port) const;
  MojoResult AddWaiter(unsigned port,
                       Waiter* waiter,
                       MojoHandleSignals signals,
                       uint32_t context,
                       HandleSignalsState* signals_state);
  void RemoveWaiter(unsigned port,
                    Waiter* waiter,
                    HandleSignalsState* signals_state);

  // This is called by the dispatcher to convert a local endpoint to a proxy
  // endpoint.
  scoped_refptr<ChannelEndpoint> ConvertLocalToProxy(unsigned port);

  // This is used by |Channel| to enqueue messages (typically to a
  // |LocalMessagePipeEndpoint|). Unlike |WriteMessage()|, |port| is the
  // *destination* port.
  MojoResult EnqueueMessage(unsigned port,
                            scoped_ptr<MessageInTransit> message);

  // This is used by |Channel|. TODO(vtl): Rename it (and have the
  // |ChannelEndpoint| call it instead).
  void OnRemove(unsigned port);

 private:
  MessagePipe();

  friend class base::RefCountedThreadSafe<MessagePipe>;
  virtual ~MessagePipe();

  // This is used internally by |WriteMessage()| and by |EnqueueMessage()|.
  // |transports| may be non-null only if it's nonempty and |message| has no
  // dispatchers attached.
  MojoResult EnqueueMessageInternal(
      unsigned port,
      scoped_ptr<MessageInTransit> message,
      std::vector<DispatcherTransport>* transports);

  // Helper for |EnqueueMessageInternal()|. Must be called with |lock_| held.
  MojoResult AttachTransportsNoLock(
      unsigned port,
      MessageInTransit* message,
      std::vector<DispatcherTransport>* transports);

  // Used by |EnqueueMessageInternal()| to handle control messages that are
  // actually meant for us.
  MojoResult HandleControlMessage(unsigned port,
                                  scoped_ptr<MessageInTransit> message);

  base::Lock lock_;  // Protects the following members.
  scoped_ptr<MessagePipeEndpoint> endpoints_[2];

  DISALLOW_COPY_AND_ASSIGN(MessagePipe);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_MESSAGE_PIPE_H_
