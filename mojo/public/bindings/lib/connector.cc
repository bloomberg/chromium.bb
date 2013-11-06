// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/connector.h"

#include <assert.h>
#include <stdlib.h>

#include <algorithm>

namespace mojo {

// ----------------------------------------------------------------------------

Connector::Connector(Handle message_pipe)
    : message_pipe_(message_pipe),
      incoming_receiver_(NULL),
      error_(false) {
}

Connector::~Connector() {
  if (read_callback_.IsPending())
    read_callback_.Cancel();
  if (write_callback_.IsPending())
    write_callback_.Cancel();
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

  write_queue_.Push(message);
  WriteMore();
  return !error_;
}

void Connector::OnHandleReady(Callback* callback, MojoResult result) {
  if (callback == &read_callback_)
    ReadMore();
  if (callback == &write_callback_)
    WriteMore();
}

void Connector::WaitToReadMore() {
  read_callback_.SetOwnerToNotify(this);

  bool ok = BindingsSupport::Get()->AsyncWait(message_pipe_,
                                              MOJO_WAIT_FLAG_READABLE,
                                              MOJO_DEADLINE_INDEFINITE,
                                              &read_callback_);
  if (!ok)
    error_ = true;
}

void Connector::WaitToWriteMore() {
  write_callback_.SetOwnerToNotify(this);

  bool ok = BindingsSupport::Get()->AsyncWait(message_pipe_,
                                              MOJO_WAIT_FLAG_WRITABLE,
                                              MOJO_DEADLINE_INDEFINITE,
                                              &write_callback_);
  if (!ok)
    error_ = true;
}

void Connector::ReadMore() {
  for (;;) {
    MojoResult rv;

    uint32_t num_bytes = 0, num_handles = 0;
    rv = ReadMessage(message_pipe_,
                     NULL,
                     &num_bytes,
                     NULL,
                     &num_handles,
                     MOJO_READ_MESSAGE_FLAG_NONE);
    if (rv == MOJO_RESULT_NOT_FOUND) {
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

    rv = ReadMessage(message_pipe_,
                     message.data,
                     &num_bytes,
                     &message.handles[0],
                     &num_handles,
                     MOJO_READ_MESSAGE_FLAG_NONE);
    if (rv != MOJO_RESULT_OK) {
      error_ = true;
      break;
    }

    incoming_receiver_->Accept(&message);
  }
}

void Connector::WriteMore() {
  while (!write_queue_.IsEmpty()) {
    const Message* message = write_queue_.Peek();

    MojoResult rv = WriteMessage(message_pipe_,
                                 message->data,
                                 message->data->header.num_bytes,
                                 message->handles.data(),
                                 message->handles.size(),
                                 MOJO_WRITE_MESSAGE_FLAG_NONE);
    if (rv == MOJO_RESULT_OK) {
      // TODO(darin): Handles were successfully transferred, and so we need
      // to take care not to Close them here.
      write_queue_.Pop();
      continue;  // Write another message.
    }

    error_ = true;
    break;
  }
}

// ----------------------------------------------------------------------------

Connector::Callback::Callback()
    : owner_(NULL) {
}

void Connector::Callback::Cancel() {
  owner_ = NULL;
  BindingsSupport::Get()->CancelWait(this);
}

void Connector::Callback::SetOwnerToNotify(Connector* owner) {
  assert(!owner_);
  owner_ = owner;
}

bool Connector::Callback::IsPending() const {
  return owner_ != NULL;
}

void Connector::Callback::OnHandleReady(MojoResult result) {
  assert(owner_);
  Connector* owner = NULL;
  std::swap(owner, owner_);
  owner->OnHandleReady(this, result);
}

}  // namespace mojo
