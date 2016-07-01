// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/bindings_internal.h"

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

}  // namespace internal
}  // namespace mojo
