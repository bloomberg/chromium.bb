// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/unserialized_message_context.h"

namespace mojo {
namespace internal {

UnserializedMessageContext::UnserializedMessageContext(const Tag* tag,
                                                       uint32_t message_name,
                                                       uint32_t message_flags)
    : tag_(tag) {
  header_.interface_id = 0;
  header_.version = 1;
  header_.name = message_name;
  header_.flags = message_flags;
}

UnserializedMessageContext::~UnserializedMessageContext() = default;

void UnserializedMessageContext::GetSerializedSize(size_t* num_bytes,
                                                   size_t* num_handles) {
  DCHECK(!serialization_context_.has_value());
  serialization_context_.emplace();

  size_t payload_size = PrepareToSerialize(&serialization_context_.value());
  total_serialized_size_ = ComputeSerializedMessageSize(
      header_.flags, payload_size, 0 /* payload_interface_id_count */);
  *num_bytes = total_serialized_size_;
  *num_handles = serialization_context_->handles()->size();
}

void UnserializedMessageContext::SerializeHandles(MojoHandle* handles) {
  DCHECK(serialization_context_.has_value());
  for (auto& handle : *serialization_context_->mutable_handles()) {
    *handles = handle.release().value();
    ++handles;
  }
}

void UnserializedMessageContext::SerializePayload(Buffer* buffer) {
  DCHECK(serialization_context_.has_value());
  Serialize(&serialization_context_.value(), buffer);
}

}  // namespace internal
}  // namespace mojo
