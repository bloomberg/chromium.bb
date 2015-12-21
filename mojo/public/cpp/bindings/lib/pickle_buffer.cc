// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/pickle_buffer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/pickle.h"
#include "mojo/public/cpp/bindings/lib/bindings_serialization.h"

namespace mojo {
namespace internal {

class PickleBuffer::Storage : public base::Pickle {
 public:
  explicit Storage(size_t num_bytes);
  ~Storage() override {}

  size_t available_capacity() const {
    return capacity_after_header() - payload_size();
  }

  void* GetData() { return static_cast<void*>(mutable_payload()); }
  void* Claim(size_t num_bytes) { return ClaimBytes(num_bytes); }

 private:
  // TODO(rockot): Stop wasting 8 bytes per buffer.
  //
  // We don't use Pickle's header at all, but its base header type consumes 4
  // bytes. We waste another 4 bytes to keep our actual buffer aligned to an
  // 8-byte boundary.
  //
  // The reason we don't use base::Pickle's header is that it stores payload
  // length in the first 4 bytes. Mojo Messages are packed like mojom structs,
  // where the first 4 bytes are header size rather than payload size.
  struct PaddedHeader : public base::Pickle::Header {
    uint32_t padding;
  };

  static_assert(sizeof(PaddedHeader) % 8 == 0,
      "PickleBuffer requires a Pickle header size with 8-byte alignment.");

  DISALLOW_COPY_AND_ASSIGN(Storage);
};

PickleBuffer::Storage::Storage(size_t num_bytes)
    : base::Pickle(sizeof(PaddedHeader)) {
  headerT<PaddedHeader>()->padding = 0;
  Resize(num_bytes);
}

PickleBuffer::PickleBuffer(size_t num_bytes, bool zero_initialized)
    : storage_(new Storage(num_bytes)) {
  if (zero_initialized)
    memset(storage_->GetData(), 0, num_bytes);
}

PickleBuffer::~PickleBuffer() {
}

const void* PickleBuffer::data() const { return storage_->GetData(); }

size_t PickleBuffer::data_num_bytes() const { return storage_->payload_size(); }

base::Pickle* PickleBuffer::pickle() const {
  return storage_.get();
}

void* PickleBuffer::Allocate(size_t num_bytes) {
  DCHECK(storage_);

  // The last allocation may terminate in between 8-byte boundaries. Pad the
  // front of this allocation if that's the case.
  size_t padded_capacity = Align(storage_->payload_size());
  DCHECK_GE(padded_capacity, storage_->payload_size());

  size_t padding_bytes = padded_capacity - storage_->payload_size();
  size_t allocation_size = padding_bytes + num_bytes;
  const void* previous_data_location = storage_->GetData();
  if (storage_->available_capacity() < allocation_size) {
    NOTREACHED() <<
        "Message buffers must be fully allocated before serialization.";
    return nullptr;
  }
  char* p = static_cast<char*>(storage_->Claim(allocation_size));
  DCHECK_EQ(storage_->GetData(), previous_data_location);
  return p + padding_bytes;
}

PickleBuffer* PickleBuffer::AsPickleBuffer() { return this; }

}  // namespace internal
}  // namespace mojo
