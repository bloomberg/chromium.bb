// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/simple_platform_support.h"

#include <stddef.h>

#include <utility>

#include "base/rand_util.h"
#include "mojo/edk/embedder/simple_platform_shared_buffer.h"

namespace mojo {
namespace edk {

void SimplePlatformSupport::GetCryptoRandomBytes(void* bytes,
                                                 size_t num_bytes) {
  base::RandBytes(bytes, num_bytes);
}

PlatformSharedBuffer* SimplePlatformSupport::CreateSharedBuffer(
    size_t num_bytes) {
  return SimplePlatformSharedBuffer::Create(num_bytes);
}

PlatformSharedBuffer* SimplePlatformSupport::CreateSharedBufferFromHandle(
    size_t num_bytes,
    ScopedPlatformHandle platform_handle) {
  return SimplePlatformSharedBuffer::CreateFromPlatformHandle(
      num_bytes, std::move(platform_handle));
}

}  // namespace edk
}  // namespace mojo
