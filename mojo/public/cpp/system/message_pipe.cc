// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/message_pipe.h"

#include "base/numerics/safe_math.h"

namespace mojo {

MojoResult WriteMessageRaw(MessagePipeHandle message_pipe,
                           const void* bytes,
                           size_t num_bytes,
                           const MojoHandle* handles,
                           size_t num_handles,
                           MojoWriteMessageFlags flags) {
  ScopedMessageHandle message_handle;
  MojoResult rv = CreateMessage(&message_handle);
  DCHECK_EQ(MOJO_RESULT_OK, rv);

  void* buffer;
  uint32_t buffer_size;
  rv = MojoAttachSerializedMessageBuffer(
      message_handle->value(), base::checked_cast<uint32_t>(num_bytes), handles,
      base::checked_cast<uint32_t>(num_handles), &buffer, &buffer_size);
  if (rv != MOJO_RESULT_OK)
    return MOJO_RESULT_ABORTED;

  DCHECK(buffer);
  DCHECK_GE(buffer_size, base::checked_cast<uint32_t>(num_bytes));
  memcpy(buffer, bytes, num_bytes);

  return MojoWriteMessage(message_pipe.value(),
                          message_handle.release().value(), flags);
}

MojoResult ReadMessageRaw(MessagePipeHandle message_pipe,
                          std::vector<uint8_t>* payload,
                          std::vector<ScopedHandle>* handles,
                          MojoReadMessageFlags flags) {
  ScopedMessageHandle message_handle;
  MojoResult rv = ReadMessageNew(message_pipe, &message_handle, flags);
  if (rv != MOJO_RESULT_OK)
    return rv;

  rv = MojoSerializeMessage(message_handle->value());
  if (rv != MOJO_RESULT_OK && rv != MOJO_RESULT_FAILED_PRECONDITION)
    return MOJO_RESULT_ABORTED;

  void* buffer = nullptr;
  uint32_t num_bytes = 0;
  uint32_t num_handles = 0;
  rv = MojoGetSerializedMessageContents(
      message_handle->value(), &buffer, &num_bytes, nullptr, &num_handles,
      MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE);
  if (rv == MOJO_RESULT_RESOURCE_EXHAUSTED) {
    DCHECK(handles);
    handles->resize(num_handles);
    rv = MojoGetSerializedMessageContents(
        message_handle->value(), &buffer, &num_bytes,
        reinterpret_cast<MojoHandle*>(handles->data()), &num_handles,
        MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE);
  }

  if (num_bytes) {
    DCHECK(buffer);
    uint8_t* payload_data = reinterpret_cast<uint8_t*>(buffer);
    payload->resize(num_bytes);
    std::copy(payload_data, payload_data + num_bytes, payload->begin());
  } else if (payload) {
    payload->clear();
  }

  if (handles && !num_handles)
    handles->clear();

  if (rv != MOJO_RESULT_OK)
    return MOJO_RESULT_ABORTED;

  return MOJO_RESULT_OK;
}

}  // namespace mojo
