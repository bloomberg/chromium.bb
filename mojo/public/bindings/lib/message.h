// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_MESSAGE_H_
#define MOJO_PUBLIC_BINDINGS_LIB_MESSAGE_H_

#include <vector>

#include "mojo/public/system/core_cpp.h"

namespace mojo {

#pragma pack(push, 1)

struct MessageHeader {
  uint32_t num_bytes;
  uint32_t name;
};
MOJO_COMPILE_ASSERT(sizeof(MessageHeader) == 8, bad_sizeof_MessageHeader);

struct MessageData {
  MessageHeader header;
  uint8_t payload[1];
};
MOJO_COMPILE_ASSERT(sizeof(MessageData) == 9, bad_sizeof_MessageData);

#pragma pack(pop)

// Message is a holder for the data and handles to be sent over a MessagePipe.
// Message owns its data and handles, but a consumer of Message is free to
// manipulate the data and handles members.
class Message {
 public:
  Message();
  ~Message();

  void Swap(Message* other);

  MessageData* data;  // Heap-allocated using malloc.
  // TODO(davemoore): Turn these into ScopedHandles and fix bindings generation.
  std::vector<Handle> handles;

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(Message);
};

class MessageReceiver {
 public:
  // The receiver may mutate the given message or take ownership of its
  // |message->data| member by setting it to NULL.  Returns true if the message
  // was accepted and false otherwise, indicating that the message was invalid
  // or malformed.
  virtual bool Accept(Message* message) = 0;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_MESSAGE_H_
