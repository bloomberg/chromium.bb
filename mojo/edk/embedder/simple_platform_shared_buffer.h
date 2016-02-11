// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_SIMPLE_PLATFORM_SHARED_BUFFER_H_
#define MOJO_EDK_EMBEDDER_SIMPLE_PLATFORM_SHARED_BUFFER_H_

#include <stddef.h>

#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/shared_memory_handle.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace edk {

// A simple implementation of |PlatformSharedBuffer| that uses
// |base::SharedMemory|.
class MOJO_SYSTEM_IMPL_EXPORT SimplePlatformSharedBuffer final
    : public PlatformSharedBuffer {
 public:
  // Creates a shared buffer of size |num_bytes| bytes (initially zero-filled).
  // |num_bytes| must be nonzero. Returns null on failure.
  static SimplePlatformSharedBuffer* Create(size_t num_bytes);

  static SimplePlatformSharedBuffer* CreateFromPlatformHandle(
      size_t num_bytes,
      ScopedPlatformHandle platform_handle);

  static SimplePlatformSharedBuffer* CreateFromSharedMemoryHandle(
      size_t num_bytes,
      bool read_only,
      base::SharedMemoryHandle handle);

  // |PlatformSharedBuffer| implementation:
  size_t GetNumBytes() const override;
  scoped_ptr<PlatformSharedBufferMapping> Map(size_t offset,
                                              size_t length) override;
  bool IsValidMap(size_t offset, size_t length) override;
  scoped_ptr<PlatformSharedBufferMapping> MapNoCheck(size_t offset,
                                                     size_t length) override;
  ScopedPlatformHandle DuplicatePlatformHandle() override;
  ScopedPlatformHandle PassPlatformHandle() override;

 private:
  explicit SimplePlatformSharedBuffer(size_t num_bytes);
  ~SimplePlatformSharedBuffer() override;

  // This is called by |Create()| before this object is given to anyone.
  bool Init();

  // This is like |Init()|, but for |CreateFromPlatformHandle()|. (Note: It
  // should verify that |platform_handle| is an appropriate handle for the
  // claimed |num_bytes_|.)
  bool InitFromPlatformHandle(ScopedPlatformHandle platform_handle);

  void InitFromSharedMemoryHandle(base::SharedMemoryHandle handle);

  const size_t num_bytes_;

  base::Lock lock_;
  scoped_ptr<base::SharedMemory> shared_memory_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(SimplePlatformSharedBuffer);
};

// An implementation of |PlatformSharedBufferMapping|, produced by
// |SimplePlatformSharedBuffer|.
class MOJO_SYSTEM_IMPL_EXPORT SimplePlatformSharedBufferMapping
    : public PlatformSharedBufferMapping {
 public:
  ~SimplePlatformSharedBufferMapping() override;

  void* GetBase() const override;
  size_t GetLength() const override;

 private:
  friend class SimplePlatformSharedBuffer;

  SimplePlatformSharedBufferMapping(base::SharedMemoryHandle handle,
                                    size_t offset,
                                    size_t length)
      : offset_(offset),
        length_(length),
        base_(nullptr),
        shared_memory_(handle, false) {}

  bool Map();
  void Unmap();

  const size_t offset_;
  const size_t length_;
  void* base_;

  // Since mapping life cycles are separate from PlatformSharedBuffer and a
  // buffer can be mapped multiple times, we have our own SharedMemory object
  // created from a duplicate handle.
  base::SharedMemory shared_memory_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(SimplePlatformSharedBufferMapping);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_SIMPLE_PLATFORM_SHARED_BUFFER_H_
