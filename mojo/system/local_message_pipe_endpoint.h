// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_LOCAL_MESSAGE_PIPE_ENDPOINT_H_
#define MOJO_SYSTEM_LOCAL_MESSAGE_PIPE_ENDPOINT_H_

#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "mojo/public/system/core.h"
#include "mojo/public/system/system_export.h"
#include "mojo/system/message_pipe_endpoint.h"
#include "mojo/system/waiter_list.h"

namespace mojo {
namespace system {

class MOJO_SYSTEM_EXPORT LocalMessagePipeEndpoint : public MessagePipeEndpoint {
 public:
  LocalMessagePipeEndpoint();
  virtual ~LocalMessagePipeEndpoint();

  // |MessagePipeEndpoint| implementation:
  virtual void Close() OVERRIDE;
  virtual bool OnPeerClose() OVERRIDE;
  virtual MojoResult CanEnqueueMessage(
      const MessageInTransit* message,
      const std::vector<Dispatcher*>* dispatchers) OVERRIDE;
  virtual void EnqueueMessage(
      MessageInTransit* message,
      std::vector<scoped_refptr<Dispatcher> >* dispatchers) OVERRIDE;

  // There's a dispatcher for |LocalMessagePipeEndpoint|s, so we have to
  // implement/override these:
  virtual void CancelAllWaiters() OVERRIDE;
  virtual MojoResult ReadMessage(
      void* bytes, uint32_t* num_bytes,
      std::vector<scoped_refptr<Dispatcher> >* dispatchers,
      uint32_t* num_dispatchers,
      MojoReadMessageFlags flags) OVERRIDE;
  virtual MojoResult AddWaiter(Waiter* waiter,
                               MojoWaitFlags flags,
                               MojoResult wake_result) OVERRIDE;
  virtual void RemoveWaiter(Waiter* waiter) OVERRIDE;

 private:
  struct MessageQueueEntry {
    MessageQueueEntry();
    // Provide an explicit copy constructor, so that we can use this directly in
    // a (C++03) STL container. However, we only allow the case where |other| is
    // empty. (We don't provide a nontrivial constructor, because it wouldn't be
    // useful with these constraints. This will change with C++11.)
    MessageQueueEntry(const MessageQueueEntry& other);
    ~MessageQueueEntry();

    MessageInTransit* message;
    std::vector<scoped_refptr<Dispatcher> > dispatchers;

   private:
    // We don't need assignment, however.
    DISALLOW_ASSIGN(MessageQueueEntry);
  };

  MojoWaitFlags SatisfiedFlags();
  MojoWaitFlags SatisfiableFlags();

  bool is_open_;
  bool is_peer_open_;

  std::deque<MessageQueueEntry> message_queue_;
  WaiterList waiter_list_;

  DISALLOW_COPY_AND_ASSIGN(LocalMessagePipeEndpoint);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_LOCAL_MESSAGE_PIPE_ENDPOINT_H_
