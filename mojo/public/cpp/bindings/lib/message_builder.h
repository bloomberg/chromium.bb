// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_BUILDER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_BUILDER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/bindings_export.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

class Message;

namespace internal {

class Buffer;

class MOJO_CPP_BINDINGS_EXPORT MessageBuilder {
 public:
  MessageBuilder(uint32_t name,
                 uint32_t flags,
                 size_t payload_size,
                 size_t payload_interface_id_count);
  ~MessageBuilder();

  Buffer* buffer() { return message_.buffer(); }
  Message* message() { return &message_; }

 private:
  void InitializeMessage(size_t size);

  Message message_;

  DISALLOW_COPY_AND_ASSIGN(MessageBuilder);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_BUILDER_H_
