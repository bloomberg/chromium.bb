// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_MESSAGE_H_
#define MOJO_PUBLIC_BINDINGS_LIB_MESSAGE_H_

#include <vector>

#include "mojo/public/system/core.h"

namespace mojo {

struct MessageHeader {
  uint32_t num_bytes;
  uint32_t name;
};

struct MessageData {
  MessageHeader header;
  uint8_t payload[1];
};

struct Message {
  Message();
  ~Message();

  MessageData* data;  // Heap-allocated.
  std::vector<Handle> handles;
};

class MessageReceiver {
 public:
  // The receiver may mutate the given message or take ownership of the its
  // contents. Upon return, if message->data is non-null, then the caller
  // regains ownership of the Message and should be responsible for freeing its
  // data member.
  virtual bool Accept(Message* message) = 0;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_MESSAGE_H_
