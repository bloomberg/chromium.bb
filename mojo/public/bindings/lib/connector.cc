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

  bool wait_to_write;
  WriteOne(message, &wait_to_write);

  if (wait_to_write) {
    WaitToWriteMore();
    if (!error_)
      write_queue_.Push(message);
  }

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
  read_callback_.SetAsyncWaitID(
      BindingsSupport::Get()->AsyncWait(message_pipe_,
                                        MOJO_WAIT_FLAG_READABLE,
                                        &read_callback_));
}

void Connector::WaitToWriteMore() {
  write_callback_.SetOwnerToNotify(this);
  write_callback_.SetAsyncWaitID(
      BindingsSupport::Get()->AsyncWait(message_pipe_,
                                        MOJO_WAIT_FLAG_WRITABLE,
                                        &write_callback_));
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
                     message.handles.empty() ? NULL : &message.handles[0],
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
  while (!error_ && !write_queue_.IsEmpty()) {
    Message* message = write_queue_.Peek();

    bool wait_to_write;
    WriteOne(message, &wait_to_write);
    if (wait_to_write)
      break;

    write_queue_.Pop();
  }
}

void Connector::WriteOne(Message* message, bool* wait_to_write) {
  // TODO(darin): WriteMessage will eventually start generating an error that
  // it cannot accept more data. In that case, we'll need to wait on the pipe
  // to determine when we can try writing again. This flag will be set to true
  // in that case.
  *wait_to_write = false;

  MojoResult rv = WriteMessage(
      message_pipe_,
      message->data,
      message->data->header.num_bytes,
      message->handles.empty() ? NULL : &message->handles[0],
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

// ----------------------------------------------------------------------------

Connector::Callback::Callback()
    : owner_(NULL),
      async_wait_id_(0) {
}

Connector::Callback::~Callback() {
  if (owner_)
    BindingsSupport::Get()->CancelWait(async_wait_id_);
}

void Connector::Callback::SetOwnerToNotify(Connector* owner) {
  assert(!owner_);
  owner_ = owner;
}

void Connector::Callback::SetAsyncWaitID(BindingsSupport::AsyncWaitID id) {
  async_wait_id_ = id;
}

void Connector::Callback::OnHandleReady(MojoResult result) {
  assert(owner_);
  Connector* owner = NULL;
  std::swap(owner, owner_);
  owner->OnHandleReady(this, result);
}

}  // namespace mojo
