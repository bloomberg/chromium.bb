// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_LOCAL_MESSAGE_PIPE_ENDPOINT_H_
#define MOJO_SYSTEM_LOCAL_MESSAGE_PIPE_ENDPOINT_H_

#include <deque>

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
  virtual MojoResult EnqueueMessage(MessageInTransit* message) OVERRIDE;

  // There's a dispatcher for |LocalMessagePipeEndpoint|s, so we have to
  // implement/override these:
  virtual void CancelAllWaiters() OVERRIDE;
  virtual MojoResult ReadMessage(void* bytes, uint32_t* num_bytes,
                                 MojoHandle* handles, uint32_t* num_handles,
                                 MojoReadMessageFlags flags) OVERRIDE;
  virtual MojoResult AddWaiter(Waiter* waiter,
                               MojoWaitFlags flags,
                               MojoResult wake_result) OVERRIDE;
  virtual void RemoveWaiter(Waiter* waiter) OVERRIDE;

 private:
  MojoWaitFlags SatisfiedFlags();
  MojoWaitFlags SatisfiableFlags();

  bool is_open_;
  bool is_peer_open_;

  std::deque<MessageInTransit*> message_queue_;
  WaiterList waiter_list_;

  DISALLOW_COPY_AND_ASSIGN(LocalMessagePipeEndpoint);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_LOCAL_MESSAGE_PIPE_ENDPOINT_H_
