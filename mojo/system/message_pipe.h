// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_MESSAGE_PIPE_H_
#define MOJO_SYSTEM_MESSAGE_PIPE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "mojo/public/c/system/core.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/message_pipe_endpoint.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

class Channel;
class Waiter;

// |MessagePipe| is the secondary object implementing a message pipe (see the
// explanatory comment in core.cc). It is typically owned by the dispatcher(s)
// corresponding to the local endpoints. This class is thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT MessagePipe :
    public base::RefCountedThreadSafe<MessagePipe> {
 public:
  MessagePipe(scoped_ptr<MessagePipeEndpoint> endpoint0,
              scoped_ptr<MessagePipeEndpoint> endpoint1);

  // Convenience constructor that constructs a |MessagePipe| with two new
  // |LocalMessagePipeEndpoint|s.
  MessagePipe();

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
                          const void* bytes,
                          uint32_t num_bytes,
                          std::vector<DispatcherTransport>* transports,
                          MojoWriteMessageFlags flags);
  // Unlike |MessagePipeDispatcher::ReadMessage()|, this does not validate its
  // arguments.
  MojoResult ReadMessage(unsigned port,
                         void* bytes,
                         uint32_t* num_bytes,
                         std::vector<scoped_refptr<Dispatcher> >* dispatchers,
                         uint32_t* num_dispatchers,
                         MojoReadMessageFlags flags);
  MojoResult AddWaiter(unsigned port,
                       Waiter* waiter,
                       MojoWaitFlags flags,
                       MojoResult wake_result);
  void RemoveWaiter(unsigned port, Waiter* waiter);

  // This is called by the dispatcher to convert a local endpoint to a proxy
  // endpoint.
  void ConvertLocalToProxy(unsigned port);

  // This is used internally by |WriteMessage()| and by |Channel| to enqueue
  // messages (typically to a |LocalMessagePipeEndpoint|). Unlike
  // |WriteMessage()|, |port| is the *destination* port. |transports| should be
  // non-null only if it's nonempty, and only if |message| has no dispatchers
  // attached.
  MojoResult EnqueueMessage(unsigned port,
                            scoped_ptr<MessageInTransit> message,
                            std::vector<DispatcherTransport>* transports);

  // These are used by |Channel|.
  void Attach(unsigned port,
              scoped_refptr<Channel> channel,
              MessageInTransit::EndpointId local_id);
  void Run(unsigned port, MessageInTransit::EndpointId remote_id);

 private:
  friend class base::RefCountedThreadSafe<MessagePipe>;
  virtual ~MessagePipe();

  // Used by |EnqueueMessage()| to handle control messages that are actually
  // meant for us.
  MojoResult HandleControlMessage(unsigned port,
                                  scoped_ptr<MessageInTransit> message);

  base::Lock lock_;  // Protects the following members.
  scoped_ptr<MessagePipeEndpoint> endpoints_[2];

  DISALLOW_COPY_AND_ASSIGN(MessagePipe);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_MESSAGE_PIPE_H_
