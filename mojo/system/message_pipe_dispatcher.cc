// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/message_pipe_dispatcher.h"

#include "base/logging.h"
#include "mojo/system/message_pipe.h"

namespace mojo {
namespace system {

MessagePipeDispatcher::MessagePipeDispatcher() {
}

void MessagePipeDispatcher::Init(scoped_refptr<MessagePipe> message_pipe,
                                 unsigned port) {
  DCHECK(message_pipe.get());
  DCHECK(port == 0 || port == 1);

  message_pipe_ = message_pipe;
  port_ = port;
}

MessagePipeDispatcher::~MessagePipeDispatcher() {
  // |Close()|/|CloseImplNoLock()| should have taken care of the pipe.
  DCHECK(!message_pipe_.get());
}

void MessagePipeDispatcher::CancelAllWaitersNoLock() {
  lock().AssertAcquired();
  message_pipe_->CancelAllWaiters(port_);
}

MojoResult MessagePipeDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  message_pipe_->Close(port_);
  message_pipe_ = NULL;
  return MOJO_RESULT_OK;
}

MojoResult MessagePipeDispatcher::WriteMessageImplNoLock(
    const void* bytes, uint32_t num_bytes,
    const MojoHandle* handles, uint32_t num_handles,
    MojoWriteMessageFlags flags) {
  lock().AssertAcquired();
  return message_pipe_->WriteMessage(port_,
                                     bytes, num_bytes,
                                     handles, num_handles,
                                     flags);
}

MojoResult MessagePipeDispatcher::ReadMessageImplNoLock(
    void* bytes, uint32_t* num_bytes,
    MojoHandle* handles, uint32_t* num_handles,
    MojoReadMessageFlags flags) {
  lock().AssertAcquired();
  return message_pipe_->ReadMessage(port_,
                                    bytes, num_bytes,
                                    handles, num_handles,
                                    flags);
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

}  // namespace system
}  // namespace mojo
