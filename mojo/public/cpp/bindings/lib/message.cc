// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/message.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>

#include "mojo/public/cpp/environment/logging.h"

namespace mojo {

Message::Message() {
}

Message::~Message() {
  CloseHandles();
}

void Message::Initialize(size_t capacity, bool zero_initialized) {
  DCHECK(!buffer_);
  buffer_.reset(new internal::PickleBuffer(capacity, zero_initialized));
}

void Message::MoveTo(Message* destination) {
  MOJO_DCHECK(this != destination);

  // No copy needed.
  std::swap(destination->buffer_, buffer_);
  std::swap(destination->handles_, handles_);

  CloseHandles();
  handles_.clear();
  buffer_.reset();
}

void Message::CloseHandles() {
  for (std::vector<Handle>::iterator it = handles_.begin();
       it != handles_.end(); ++it) {
    if (it->is_valid())
      CloseRaw(*it);
  }
}

MojoResult ReadMessage(MessagePipeHandle handle, Message* message) {
  MojoResult rv;

  uint32_t num_bytes = 0, num_handles = 0;
  rv = ReadMessageRaw(handle,
                      nullptr,
                      &num_bytes,
                      nullptr,
                      &num_handles,
                      MOJO_READ_MESSAGE_FLAG_NONE);
  if (rv != MOJO_RESULT_RESOURCE_EXHAUSTED)
    return rv;

  message->Initialize(num_bytes, false /* zero_initialized */);

  void* mutable_data = message->buffer()->Allocate(num_bytes);
  message->mutable_handles()->resize(num_handles);

  rv = ReadMessageRaw(
      handle, mutable_data, &num_bytes,
      message->mutable_handles()->empty()
          ? nullptr
          : reinterpret_cast<MojoHandle*>(message->mutable_handles()->data()),
      &num_handles, MOJO_READ_MESSAGE_FLAG_NONE);
  return rv;
}

}  // namespace mojo
