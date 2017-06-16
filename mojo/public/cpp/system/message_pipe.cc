// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

namespace {

struct RawMessage {
  RawMessage(const void* bytes,
             size_t num_bytes,
             const MojoHandle* handles,
             size_t num_handles)
      : bytes(static_cast<const uint8_t*>(bytes)),
        num_bytes(num_bytes),
        handles(handles),
        num_handles(num_handles) {}

  const uint8_t* const bytes;
  const size_t num_bytes;
  const MojoHandle* const handles;
  const size_t num_handles;
};

void GetRawMessageSize(uintptr_t context,
                       size_t* num_bytes,
                       size_t* num_handles) {
  auto* message = reinterpret_cast<RawMessage*>(context);
  *num_bytes = message->num_bytes;
  *num_handles = message->num_handles;
}

void SerializeRawMessageHandles(uintptr_t context, MojoHandle* handles) {
  auto* message = reinterpret_cast<RawMessage*>(context);
  DCHECK(message->handles);
  DCHECK(message->num_handles);
  std::copy(message->handles, message->handles + message->num_handles, handles);
}

void SerializeRawMessagePayload(uintptr_t context, void* buffer) {
  auto* message = reinterpret_cast<RawMessage*>(context);
  DCHECK(message->bytes);
  DCHECK(message->num_bytes);
  std::copy(message->bytes, message->bytes + message->num_bytes,
            static_cast<uint8_t*>(buffer));
}

void DoNothing(uintptr_t context) {}

const MojoMessageOperationThunks kRawMessageThunks = {
    sizeof(kRawMessageThunks),
    &GetRawMessageSize,
    &SerializeRawMessageHandles,
    &SerializeRawMessagePayload,
    &DoNothing,
};

}  // namespace

MojoResult WriteMessageRaw(MessagePipeHandle message_pipe,
                           const void* bytes,
                           size_t num_bytes,
                           const MojoHandle* handles,
                           size_t num_handles,
                           MojoWriteMessageFlags flags) {
  RawMessage message(bytes, num_bytes, handles, num_handles);
  ScopedMessageHandle message_handle;
  MojoResult rv = CreateMessage(reinterpret_cast<uintptr_t>(&message),
                                &kRawMessageThunks, &message_handle);
  DCHECK_EQ(MOJO_RESULT_OK, rv);

  // Force the message object to be serialized immediately so we can copy the
  // local data in.
  if (MojoSerializeMessage(message_handle->value()) != MOJO_RESULT_OK) {
    // If serialization fails for some reason (e.g. invalid handles) we must
    // be careful to not propagate the message object further. It is unsafe for
    // the message's unserialized context to persist beyond the scope of this
    // function.
    return MOJO_RESULT_ABORTED;
  }

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
