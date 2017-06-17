// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"

#include <stdlib.h>

#include "mojo/public/cpp/bindings/lib/bindings_internal.h"

namespace mojo {
namespace internal {

FixedBufferForTesting::FixedBufferForTesting(size_t size)
    : FixedBufferForTesting(nullptr, Align(size)) {}

FixedBufferForTesting::~FixedBufferForTesting() {
  free(data());
}

void* FixedBufferForTesting::Leak() {
  void* ptr = data();
  Reset();
  return ptr;
}

FixedBufferForTesting::FixedBufferForTesting(std::nullptr_t,
                                             size_t aligned_size)
    : Buffer(calloc(aligned_size, 1), aligned_size) {}

}  // namespace internal
}  // namespace mojo
