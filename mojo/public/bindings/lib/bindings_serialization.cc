// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/bindings_serialization.h"

#include <assert.h>

namespace mojo {
namespace internal {

size_t Align(size_t size) {
  const size_t kAlignment = 8;
  return size + (kAlignment - (size % kAlignment)) % kAlignment;
}

void EncodePointer(const void* ptr, uint64_t* offset) {
  if (!ptr) {
    *offset = 0;
    return;
  }

  const char* p_obj = reinterpret_cast<const char*>(ptr);
  const char* p_slot = reinterpret_cast<const char*>(offset);
  assert(p_obj > p_slot);

  *offset = static_cast<uint64_t>(p_obj - p_slot);
}

const void* DecodePointerRaw(const uint64_t* offset) {
  if (!*offset)
    return NULL;
  return reinterpret_cast<const char*>(offset) + *offset;
}

bool ValidatePointer(const void* ptr, const Message& message) {
  const uint8_t* data = static_cast<const uint8_t*>(ptr);
  if (reinterpret_cast<ptrdiff_t>(data) % 8 != 0)
    return false;

  const uint8_t* data_start = reinterpret_cast<const uint8_t*>(message.data);
  const uint8_t* data_end = data_start + message.data->header.num_bytes;

  return data >= data_start && data < data_end;
}

void EncodeHandle(Handle* handle, std::vector<Handle>* handles) {
  handles->push_back(*handle);
  handle->set_value(static_cast<MojoHandle>(handles->size() - 1));
}

bool DecodeHandle(Handle* handle, const std::vector<Handle>& handles) {
  if (handle->value() >= handles.size())
    return false;
  *handle = handles[handle->value()];
  return true;
}

// static
void ArrayHelper<Handle>::EncodePointersAndHandles(
    const ArrayHeader* header,
    ElementType* elements,
    std::vector<Handle>* handles) {
  for (uint32_t i = 0; i < header->num_elements; ++i)
    EncodeHandle(&elements[i], handles);
}

// static
bool ArrayHelper<Handle>::DecodePointersAndHandles(
    const ArrayHeader* header,
    ElementType* elements,
    const Message& message) {
  for (uint32_t i = 0; i < header->num_elements; ++i) {
    if (!DecodeHandle(&elements[i], message.handles))
      return false;
  }
  return true;
}

}  // namespace internal
}  // namespace mojo
