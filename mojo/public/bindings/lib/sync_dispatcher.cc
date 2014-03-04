// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/sync_dispatcher.h"

#include <stdlib.h>

#include "mojo/public/bindings/message.h"

namespace mojo {

bool WaitForMessageAndDispatch(MessagePipeHandle handle,
                               mojo::MessageReceiver* receiver) {
  uint32_t num_bytes = 0, num_handles = 0;
  while (true) {
    MojoResult rv = ReadMessageRaw(handle,
                                   NULL,
                                   &num_bytes,
                                   NULL,
                                   &num_handles,
                                   MOJO_READ_MESSAGE_FLAG_NONE);
    if (rv == MOJO_RESULT_RESOURCE_EXHAUSTED)
      break;
    if (rv != MOJO_RESULT_SHOULD_WAIT)
      return false;
    rv = Wait(handle, MOJO_WAIT_FLAG_READABLE, MOJO_DEADLINE_INDEFINITE);
    if (rv != MOJO_RESULT_OK)
      return false;
  }

  Message message;
  message.AllocData(num_bytes);
  message.mutable_handles()->resize(num_handles);

  MojoResult rv = ReadMessageRaw(
      handle,
      message.mutable_data(),
      &num_bytes,
      message.mutable_handles()->empty() ? NULL :
          reinterpret_cast<MojoHandle*>(&message.mutable_handles()->front()),
      &num_handles,
      MOJO_READ_MESSAGE_FLAG_NONE);
  if (rv != MOJO_RESULT_OK)
    return false;
  return receiver->Accept(&message);
}

}  // namespace mojo
