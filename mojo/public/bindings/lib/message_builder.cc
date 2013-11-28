// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/message_builder.h"

#include "mojo/public/bindings/lib/message.h"

namespace mojo {
namespace internal {

MessageBuilder::MessageBuilder(uint32_t message_name, size_t payload_size)
    : buf_(sizeof(MessageHeader) + payload_size) {
  MessageHeader* header =
      static_cast<MessageHeader*>(buf_.Allocate(sizeof(MessageHeader)));
  header->num_bytes = static_cast<uint32_t>(buf_.size());
  header->name = message_name;
}

MessageBuilder::~MessageBuilder() {
}

MessageData* MessageBuilder::Finish() {
  return static_cast<MessageData*>(buf_.Leak());
}

}  // namespace internal
}  // namespace mojo
