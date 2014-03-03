// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_MESSAGE_BUILDER_H_
#define MOJO_PUBLIC_BINDINGS_LIB_MESSAGE_BUILDER_H_

#include <stdint.h>

#include "mojo/public/bindings/lib/fixed_buffer.h"

namespace mojo {
class Message;

namespace internal {

class MessageBuilder {
 public:
  MessageBuilder(uint32_t name, size_t payload_size);
  ~MessageBuilder();

  Buffer* buffer() { return &buf_; }

  // Call Finish when done making allocations in |buffer()|. A heap-allocated
  // MessageData object will be returned. When no longer needed, use |free()|
  // to release the MessageData object's memory.
  void Finish(Message* message);

 protected:
  explicit MessageBuilder(size_t size);
  FixedBuffer buf_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(MessageBuilder);
};

class MessageWithRequestIDBuilder : public MessageBuilder {
 public:
  MessageWithRequestIDBuilder(uint32_t name, size_t payload_size,
                              uint32_t flags, uint64_t request_id);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_MESSAGE_BUILDER_H_
