// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/connector.h"

#include <stdlib.h>

#include "mojo/public/bindings/error_handler.h"

namespace mojo {
namespace internal {

// ----------------------------------------------------------------------------

Connector::Connector(ScopedMessagePipeHandle message_pipe,
                     MojoAsyncWaiter* waiter)
    : error_handler_(NULL),
      waiter_(waiter),
      message_pipe_(message_pipe.Pass()),
      incoming_receiver_(NULL),
      async_wait_id_(0),
      error_(false),
      drop_writes_(false) {
  // Even though we don't have an incoming receiver, we still want to monitor
  // the message pipe to know if is closed or encounters an error.
  WaitToReadMore();
}

Connector::~Connector() {
  if (async_wait_id_)
    waiter_->CancelWait(waiter_, async_wait_id_);
}

bool Connector::Accept(Message* message) {
  if (error_)
    return false;

  if (drop_writes_)
    return true;

  MojoResult rv = WriteMessageRaw(
      message_pipe_.get(),
      message->data(),
      message->data()->header.num_bytes,
      message->mutable_handles()->empty() ? NULL :
          reinterpret_cast<const MojoHandle*>(
              &message->mutable_handles()->front()),
      static_cast<uint32_t>(message->mutable_handles()->size()),
      MOJO_WRITE_MESSAGE_FLAG_NONE);

  switch (rv) {
    case MOJO_RESULT_OK:
      // The handles were successfully transferred, so we don't need the message
      // to track their lifetime any longer.
      message->mutable_handles()->clear();
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // There's no point in continuing to write to this pipe since the other
      // end is gone. Avoid writing any future messages. Hide write failures
      // from the caller since we'd like them to continue consuming any backlog
      // of incoming messages before regarding the message pipe as closed.
      drop_writes_ = true;
      break;
    default:
      // This particular write was rejected, presumably because of bad input.
      // The pipe is not necessarily in a bad state.
      return false;
  }
  return true;
}

// static
void Connector::CallOnHandleReady(void* closure, MojoResult result) {
  Connector* self = static_cast<Connector*>(closure);
  self->OnHandleReady(result);
}

void Connector::OnHandleReady(MojoResult result) {
  async_wait_id_ = 0;

  if (result == MOJO_RESULT_OK) {
    ReadMore();
  } else {
    error_ = true;
  }

  if (error_ && error_handler_)
    error_handler_->OnError();
}

void Connector::WaitToReadMore() {
  async_wait_id_ = waiter_->AsyncWait(waiter_,
                                      message_pipe_.get().value(),
                                      MOJO_WAIT_FLAG_READABLE,
                                      MOJO_DEADLINE_INDEFINITE,
                                      &Connector::CallOnHandleReady,
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
    message.AllocData(num_bytes);
    message.mutable_handles()->resize(num_handles);

    rv = ReadMessageRaw(
        message_pipe_.get(),
        message.mutable_data(),
        &num_bytes,
        message.mutable_handles()->empty() ? NULL :
            reinterpret_cast<MojoHandle*>(&message.mutable_handles()->front()),
        &num_handles,
        MOJO_READ_MESSAGE_FLAG_NONE);
    if (rv != MOJO_RESULT_OK) {
      error_ = true;
      break;
    }

    if (incoming_receiver_)
      incoming_receiver_->Accept(&message);
  }
}

}  // namespace internal
}  // namespace mojo
