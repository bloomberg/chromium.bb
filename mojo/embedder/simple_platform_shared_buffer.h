// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_SIMPLE_PLATFORM_SHARED_BUFFER_H_
#define MOJO_SYSTEM_SIMPLE_PLATFORM_SHARED_BUFFER_H_

#include <stddef.h>

#include "base/macros.h"
#include "mojo/embedder/platform_shared_buffer.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace embedder {

// A simple implementation of |PlatformSharedBuffer|.
class MOJO_SYSTEM_IMPL_EXPORT SimplePlatformSharedBuffer
    : public PlatformSharedBuffer {
 public:
  // Creates a shared buffer of size |num_bytes| bytes (initially zero-filled).
  // |num_bytes| must be nonzero. Returns null on failure.
  static SimplePlatformSharedBuffer* Create(size_t num_bytes);

  static SimplePlatformSharedBuffer* CreateFromPlatformHandle(
      size_t num_bytes,
      ScopedPlatformHandle platform_handle);

  // |PlatformSharedBuffer| implementation:
  virtual size_t GetNumBytes() const OVERRIDE;
  virtual scoped_ptr<PlatformSharedBufferMapping> Map(size_t offset,
                                                      size_t length) OVERRIDE;
  virtual bool IsValidMap(size_t offset, size_t length) OVERRIDE;
  virtual scoped_ptr<PlatformSharedBufferMapping> MapNoCheck(
      size_t offset,
      size_t length) OVERRIDE;
  virtual ScopedPlatformHandle DuplicatePlatformHandle() OVERRIDE;
  virtual ScopedPlatformHandle PassPlatformHandle() OVERRIDE;

 private:
  explicit SimplePlatformSharedBuffer(size_t num_bytes);
  virtual ~SimplePlatformSharedBuffer();

  // Implemented in simple_platform_shared_buffer_{posix,win}.cc:

  // This is called by |Create()| before this object is given to anyone.
  bool Init();

  // This is like |Init()|, but for |CreateFromPlatformHandle()|. (Note: It
  // should verify that |platform_handle| is an appropriate handle for the
  // claimed |num_bytes_|.)
  bool InitFromPlatformHandle(ScopedPlatformHandle platform_handle);

  // The platform-dependent part of |Map()|; doesn't check arguments.
  scoped_ptr<PlatformSharedBufferMapping> MapImpl(size_t offset, size_t length);

  const size_t num_bytes_;

  // This is set in |Init()|/|InitFromPlatformHandle()| and never modified
  // (except by |PassPlatformHandle()|; see the comments above its declaration),
  // hence does not need to be protected by a lock.
  ScopedPlatformHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(SimplePlatformSharedBuffer);
};

// An implementation of |PlatformSharedBufferMapping|, produced by
// |SimplePlatformSharedBuffer|.
class MOJO_SYSTEM_IMPL_EXPORT SimplePlatformSharedBufferMapping
    : public PlatformSharedBufferMapping {
 public:
  virtual ~SimplePlatformSharedBufferMapping();

  virtual void* GetBase() const OVERRIDE;
  virtual size_t GetLength() const OVERRIDE;

 private:
  friend class SimplePlatformSharedBuffer;

  SimplePlatformSharedBufferMapping(void* base,
                                    size_t length,
                                    void* real_base,
                                    size_t real_length)
      : base_(base),
        length_(length),
        real_base_(real_base),
        real_length_(real_length) {}
  void Unmap();

  void* const base_;
  const size_t length_;

  void* const real_base_;
  const size_t real_length_;

  DISALLOW_COPY_AND_ASSIGN(SimplePlatformSharedBufferMapping);
};

}  // namespace embedder
}  // namespace mojo

#endif  // MOJO_SYSTEM_SIMPLE_PLATFORM_SHARED_BUFFER_H_
