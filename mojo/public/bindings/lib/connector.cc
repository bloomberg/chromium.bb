// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/connector.h"

#include <assert.h>
#include <stdlib.h>

#include <algorithm>

namespace mojo {
namespace internal {

// ----------------------------------------------------------------------------

Connector::Connector(ScopedMessagePipeHandle message_pipe,
                     MojoAsyncWaiter* waiter)
    : waiter_(waiter),
      message_pipe_(message_pipe.Pass()),
      incoming_receiver_(NULL),
      async_wait_id_(0),
      error_(false) {
}

Connector::~Connector() {
  if (async_wait_id_)
    waiter_->CancelWait(waiter_, async_wait_id_);
}

void Connector::SetIncomingReceiver(MessageReceiver* receiver) {
  assert(!incoming_receiver_);
  incoming_receiver_ = receiver;
  if (incoming_receiver_)
    WaitToReadMore();
}

bool Connector::Accept(Message* message) {
  if (error_)
    return false;

  WriteOne(message);
  return !error_;
}

// static
void Connector::OnHandleReady(void* closure, MojoResult result) {
  Connector* self = static_cast<Connector*>(closure);
  self->async_wait_id_ = 0;
  self->ReadMore();
}

void Connector::WaitToReadMore() {
  async_wait_id_ = waiter_->AsyncWait(waiter_,
                                      message_pipe_.get().value(),
                                      MOJO_WAIT_FLAG_READABLE,
                                      MOJO_DEADLINE_INDEFINITE,
                                      &Connector::OnHandleReady,
                                      this);
}

void Connector::ReadMore() {
  for (;;) {
    MojoResult rv;

    uint32_t num_bytes = 0, num_handles = 0;
    rv = ReadMessageRaw(message_pipe_.get(),
                        NULL,
                        &num_bytes,
                        NULL,
                        &num_handles,
                        MOJO_READ_MESSAGE_FLAG_NONE);
    if (rv == MOJO_RESULT_SHOULD_WAIT) {
      WaitToReadMore();
      break;
    }
    if (rv != MOJO_RESULT_RESOURCE_EXHAUSTED) {
      error_ = true;
      break;
    }

    Message message;
    message.data = static_cast<MessageData*>(malloc(num_bytes));
    message.handles.resize(num_handles);

    rv = ReadMessageRaw(message_pipe_.get(),
                        message.data,
                        &num_bytes,
                        message.handles.empty() ? NULL :
                            reinterpret_cast<MojoHandle*>(&message.handles[0]),
                        &num_handles,
                        MOJO_READ_MESSAGE_FLAG_NONE);
    if (rv != MOJO_RESULT_OK) {
      error_ = true;
      break;
    }

    incoming_receiver_->Accept(&message);
  }
}

void Connector::WriteOne(Message* message) {
  MojoResult rv = WriteMessageRaw(
      message_pipe_.get(),
      message->data,
      message->data->header.num_bytes,
      message->handles.empty() ? NULL :
          reinterpret_cast<const MojoHandle*>(&message->handles[0]),
      static_cast<uint32_t>(message->handles.size()),
      MOJO_WRITE_MESSAGE_FLAG_NONE);
  if (rv == MOJO_RESULT_OK) {
    // The handles were successfully transferred, so we don't need the message
    // to track their lifetime any longer.
    message->handles.clear();
  } else {
    error_ = true;
  }
}

}  // namespace internal
}  // namespace mojo
