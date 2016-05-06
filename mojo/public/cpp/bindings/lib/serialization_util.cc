// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/serialization_util.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/logging.h"

namespace mojo {
namespace internal {

namespace {

const size_t kAlignment = 8;

template <typename T>
T AlignImpl(T t) {
  return t + (kAlignment - (t % kAlignment)) % kAlignment;
}

}  // namespace

size_t Align(size_t size) {
  return AlignImpl(size);
}

char* AlignPointer(char* ptr) {
  return reinterpret_cast<char*>(AlignImpl(reinterpret_cast<uintptr_t>(ptr)));
}

bool IsAligned(const void* ptr) {
  return !(reinterpret_cast<uintptr_t>(ptr) % kAlignment);
}

void EncodePointer(const void* ptr, uint64_t* offset) {
  if (!ptr) {
    *offset = 0;
    return;
  }

  const char* p_obj = reinterpret_cast<const char*>(ptr);
  const char* p_slot = reinterpret_cast<const char*>(offset);
  DCHECK(p_obj > p_slot);

  *offset = static_cast<uint64_t>(p_obj - p_slot);
}

const void* DecodePointerRaw(const uint64_t* offset) {
  if (!*offset)
    return nullptr;
  return reinterpret_cast<const char*>(offset) + *offset;
}

SerializedHandleVector::SerializedHandleVector() {}

SerializedHandleVector::~SerializedHandleVector() {
  for (auto handle : handles_) {
    if (handle.is_valid()) {
      MojoResult rv = MojoClose(handle.value());
      DCHECK_EQ(rv, MOJO_RESULT_OK);
    }
  }
}

Handle_Data SerializedHandleVector::AddHandle(mojo::Handle handle) {
  Handle_Data data;
  if (!handle.is_valid()) {
    data.value = kEncodedInvalidHandleValue;
  } else {
    DCHECK_LT(handles_.size(), std::numeric_limits<uint32_t>::max());
    data.value = static_cast<uint32_t>(handles_.size());
    handles_.push_back(handle);
  }
  return data;
}

mojo::Handle SerializedHandleVector::TakeHandle(
    const Handle_Data& encoded_handle) {
  if (!encoded_handle.is_valid())
    return mojo::Handle();
  DCHECK_LT(encoded_handle.value, handles_.size());
  return FetchAndReset(&handles_[encoded_handle.value]);
}

void SerializedHandleVector::Swap(std::vector<mojo::Handle>* other) {
  handles_.swap(*other);
}

SerializationContext::SerializationContext() {}

SerializationContext::SerializationContext(
    scoped_refptr<MultiplexRouter> in_router)
    : router(std::move(in_router)) {}

SerializationContext::~SerializationContext() {
  DCHECK(!custom_contexts || custom_contexts->empty());
}

}  // namespace internal
}  // namespace mojo
