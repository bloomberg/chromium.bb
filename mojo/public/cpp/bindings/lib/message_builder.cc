// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/message_builder.h"

#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/buffer.h"
#include "mojo/public/cpp/bindings/lib/message_internal.h"

namespace mojo {
namespace internal {

template <typename Header>
void Allocate(Buffer* buf, Header** header) {
  *header = static_cast<Header*>(buf->Allocate(sizeof(Header)));
  (*header)->num_bytes = sizeof(Header);
}

MessageBuilder::MessageBuilder(uint32_t name,
                               uint32_t flags,
                               size_t payload_size,
                               size_t payload_interface_id_count) {
  if (payload_interface_id_count > 0) {
    // Version 2
    InitializeMessage(
        sizeof(MessageHeaderV2) + Align(payload_size) +
        ArrayDataTraits<uint32_t>::GetStorageSize(
            static_cast<uint32_t>(payload_interface_id_count)));

    MessageHeaderV2* header;
    Allocate(message_.buffer(), &header);
    header->version = 2;
    header->name = name;
    header->flags = flags;
    // The payload immediately follows the header.
    header->payload.Set(header + 1);
  } else if (flags &
             (Message::kFlagExpectsResponse | Message::kFlagIsResponse)) {
    // Version 1
    InitializeMessage(sizeof(MessageHeaderV1) + payload_size);

    MessageHeaderV1* header;
    Allocate(message_.buffer(), &header);
    header->version = 1;
    header->name = name;
    header->flags = flags;
  } else {
    InitializeMessage(sizeof(MessageHeader) + payload_size);

    MessageHeader* header;
    Allocate(message_.buffer(), &header);
    header->version = 0;
    header->name = name;
    header->flags = flags;
  }
}

MessageBuilder::~MessageBuilder() {
}

void MessageBuilder::InitializeMessage(size_t size) {
  message_.Initialize(static_cast<uint32_t>(Align(size)),
                      true /* zero_initialized */);
}

}  // namespace internal
}  // namespace mojo
