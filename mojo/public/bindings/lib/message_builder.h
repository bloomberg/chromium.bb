// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_MESSAGE_BUILDER_H_
#define MOJO_PUBLIC_BINDINGS_LIB_MESSAGE_BUILDER_H_

#include <stdint.h>

#include "mojo/public/bindings/lib/buffer.h"

namespace mojo {
struct MessageData;

// Used to construct a MessageData object.
class MessageBuilder {
 public:
  MessageBuilder(uint32_t message_name, size_t payload_size);
  ~MessageBuilder();

  Buffer* buffer() { return &buf_; }

  // Call Finish when done making allocations in |buffer()|. A heap-allocated
  // MessageData object will be returned. When no longer needed, use |free()|
  // to release the MessageData object's memory.
  MessageData* Finish();

 private:
  FixedBuffer buf_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(MessageBuilder);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_MESSAGE_BUILDER_H_
