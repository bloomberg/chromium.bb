// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_MESSAGE_PIPE_ENDPOINT_H_
#define MOJO_SYSTEM_MESSAGE_PIPE_ENDPOINT_H_

#include <stdint.h>

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/system/core.h"
#include "mojo/system/message_in_transit.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

class Channel;
class Dispatcher;
class Waiter;

// This is an interface to one of the ends of a message pipe, and is used by
// |MessagePipe|. Its most important role is to provide a sink for messages
// (i.e., a place where messages can be sent). It has a secondary role: When the
// endpoint is local (i.e., in the current process), there'll be a dispatcher
// corresponding to the endpoint. In that case, the implementation of
// |MessagePipeEndpoint| also implements the functionality required by the
// dispatcher, e.g., to read messages and to wait. Implementations of this class
// are not thread-safe; instances are protected by |MesssagePipe|'s lock.
class MOJO_SYSTEM_IMPL_EXPORT MessagePipeEndpoint {
 public:
  virtual ~MessagePipeEndpoint() {}

  // All implementations must implement these.
  virtual void Close() = 0;
  virtual void OnPeerClose() = 0;
  // Checks if |EnqueueMessage()| will be able to enqueue the given message
  // (with the given set of dispatchers). |dispatchers| should be non-null only
  // if it's nonempty. Returns |MOJO_RESULT_OK| if it will and an appropriate
  // error code if it won't.
  virtual MojoResult CanEnqueueMessage(
      const MessageInTransit* message,
      const std::vector<Dispatcher*>* dispatchers) = 0;
  // Takes ownership of |message| and the contents of |dispatchers| (leaving
  // it empty). This should only be called after |CanEnqueueMessage()| has
  // indicated success. (Unlike |CanEnqueueMessage()|, |dispatchers| may be
  // non-null but empty.)
  virtual void EnqueueMessage(
      MessageInTransit* message,
      std::vector<scoped_refptr<Dispatcher> >* dispatchers) = 0;

  // Implementations must override these if they represent a local endpoint,
  // i.e., one for which there's a |MessagePipeDispatcher| (and thus a handle).
  // An implementation for a proxy endpoint (for which there's no dispatcher)
  // needs not override these methods, since they should never be called.
  //
  // These methods implement the methods of the same name in |MessagePipe|,
  // though |MessagePipe|'s implementation may have to do a little more if the
  // operation involves both endpoints.
  virtual void CancelAllWaiters();
  virtual MojoResult ReadMessage(
      void* bytes, uint32_t* num_bytes,
      std::vector<scoped_refptr<Dispatcher> >* dispatchers,
      uint32_t* num_dispatchers,
      MojoReadMessageFlags flags);
  virtual MojoResult AddWaiter(Waiter* waiter,
                               MojoWaitFlags flags,
                               MojoResult wake_result);
  virtual void RemoveWaiter(Waiter* waiter);

  // Implementations must override these if they represent a proxy endpoint. An
  // implementation for a local endpoint needs not override these methods, since
  // they should never be called.
  virtual void Attach(scoped_refptr<Channel> channel,
                      MessageInTransit::EndpointId local_id);
  virtual void Run(MessageInTransit::EndpointId remote_id);

 protected:
  MessagePipeEndpoint() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MessagePipeEndpoint);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_MESSAGE_PIPE_ENDPOINT_H_
