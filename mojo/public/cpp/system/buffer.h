// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_BUFFER_H_
#define MOJO_PUBLIC_CPP_SYSTEM_BUFFER_H_

#include <assert.h>

#include "mojo/public/c/system/buffer.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

// SharedBufferHandle ----------------------------------------------------------

class SharedBufferHandle : public Handle {
 public:
  SharedBufferHandle() {}
  explicit SharedBufferHandle(MojoHandle value) : Handle(value) {}

  // Copying and assignment allowed.
};

static_assert(sizeof(SharedBufferHandle) == sizeof(Handle),
              "Bad size for C++ SharedBufferHandle");

typedef ScopedHandleBase<SharedBufferHandle> ScopedSharedBufferHandle;
static_assert(sizeof(ScopedSharedBufferHandle) == sizeof(SharedBufferHandle),
              "Bad size for C++ ScopedSharedBufferHandle");

inline MojoResult CreateSharedBuffer(
    const MojoCreateSharedBufferOptions* options,
    uint64_t num_bytes,
    ScopedSharedBufferHandle* shared_buffer) {
  assert(shared_buffer);
  SharedBufferHandle handle;
  MojoResult rv =
      MojoCreateSharedBuffer(options, num_bytes, handle.mutable_value());
  // Reset even on failure (reduces the chances that a "stale"/incorrect handle
  // will be used).
  shared_buffer->reset(handle);
  return rv;
}

// TODO(vtl): This (and also the functions below) are templatized to allow for
// future/other buffer types. A bit "safer" would be to overload this function
// manually. (The template enforces that the in and out handles to be of the
// same type.)
template <class BufferHandleType>
inline MojoResult DuplicateBuffer(
    BufferHandleType buffer,
    const MojoDuplicateBufferHandleOptions* options,
    ScopedHandleBase<BufferHandleType>* new_buffer) {
  assert(new_buffer);
  BufferHandleType handle;
  MojoResult rv = MojoDuplicateBufferHandle(
      buffer.value(), options, handle.mutable_value());
  // Reset even on failure (reduces the chances that a "stale"/incorrect handle
  // will be used).
  new_buffer->reset(handle);
  return rv;
}

template <class BufferHandleType>
inline MojoResult MapBuffer(BufferHandleType buffer,
                            uint64_t offset,
                            uint64_t num_bytes,
                            void** pointer,
                            MojoMapBufferFlags flags) {
  assert(buffer.is_valid());
  return MojoMapBuffer(buffer.value(), offset, num_bytes, pointer, flags);
}

inline MojoResult UnmapBuffer(void* pointer) {
  assert(pointer);
  return MojoUnmapBuffer(pointer);
}

// A wrapper class that automatically creates a shared buffer and owns the
// handle.
class SharedBuffer {
 public:
  explicit SharedBuffer(uint64_t num_bytes);
  SharedBuffer(uint64_t num_bytes,
               const MojoCreateSharedBufferOptions& options);
  ~SharedBuffer();

  ScopedSharedBufferHandle handle;
};

inline SharedBuffer::SharedBuffer(uint64_t num_bytes) {
  MojoResult result MOJO_ALLOW_UNUSED =
      CreateSharedBuffer(nullptr, num_bytes, &handle);
  assert(result == MOJO_RESULT_OK);
}

inline SharedBuffer::SharedBuffer(
    uint64_t num_bytes,
    const MojoCreateSharedBufferOptions& options) {
  MojoResult result MOJO_ALLOW_UNUSED =
      CreateSharedBuffer(&options, num_bytes, &handle);
  assert(result == MOJO_RESULT_OK);
}

inline SharedBuffer::~SharedBuffer() {
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_BUFFER_H_
