// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/embedder/simple_platform_support.h"

#include "mojo/embedder/simple_platform_shared_buffer.h"

namespace mojo {
namespace embedder {

PlatformSharedBuffer* SimplePlatformSupport::CreateSharedBuffer(
    size_t num_bytes) {
  return SimplePlatformSharedBuffer::Create(num_bytes);
}

PlatformSharedBuffer* SimplePlatformSupport::CreateSharedBufferFromHandle(
    size_t num_bytes,
    ScopedPlatformHandle platform_handle) {
  return SimplePlatformSharedBuffer::CreateFromPlatformHandle(
      num_bytes, platform_handle.Pass());
}

}  // namespace embedder
}  // namespace mojo
