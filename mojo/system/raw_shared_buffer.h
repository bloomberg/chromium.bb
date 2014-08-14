// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_RAW_SHARED_BUFFER_H_
#define MOJO_SYSTEM_RAW_SHARED_BUFFER_H_

#include <stddef.h>

#include "base/macros.h"
#include "mojo/embedder/platform_shared_buffer.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

// A simple implementation of |embedder::PlatformSharedBuffer|.
class MOJO_SYSTEM_IMPL_EXPORT RawSharedBuffer
    : public embedder::PlatformSharedBuffer {
 public:
  // Creates a shared buffer of size |num_bytes| bytes (initially zero-filled).
  // |num_bytes| must be nonzero. Returns null on failure.
  static RawSharedBuffer* Create(size_t num_bytes);

  static RawSharedBuffer* CreateFromPlatformHandle(
      size_t num_bytes,
      embedder::ScopedPlatformHandle platform_handle);

  // |embedder::PlatformSharedBuffer| implementation:
  virtual size_t GetNumBytes() const OVERRIDE;
  virtual scoped_ptr<embedder::PlatformSharedBufferMapping> Map(
      size_t offset,
      size_t length) OVERRIDE;
  virtual bool IsValidMap(size_t offset, size_t length) OVERRIDE;
  virtual scoped_ptr<embedder::PlatformSharedBufferMapping> MapNoCheck(
      size_t offset,
      size_t length) OVERRIDE;
  virtual embedder::ScopedPlatformHandle DuplicatePlatformHandle() OVERRIDE;
  virtual embedder::ScopedPlatformHandle PassPlatformHandle() OVERRIDE;

 private:
  explicit RawSharedBuffer(size_t num_bytes);
  virtual ~RawSharedBuffer();

  // Implemented in raw_shared_buffer_{posix,win}.cc:

  // This is called by |Create()| before this object is given to anyone.
  bool Init();

  // This is like |Init()|, but for |CreateFromPlatformHandle()|. (Note: It
  // should verify that |platform_handle| is an appropriate handle for the
  // claimed |num_bytes_|.)
  bool InitFromPlatformHandle(embedder::ScopedPlatformHandle platform_handle);

  // The platform-dependent part of |Map()|; doesn't check arguments.
  scoped_ptr<embedder::PlatformSharedBufferMapping> MapImpl(size_t offset,
                                                            size_t length);

  const size_t num_bytes_;

  // This is set in |Init()|/|InitFromPlatformHandle()| and never modified
  // (except by |PassPlatformHandle()|; see the comments above its declaration),
  // hence does not need to be protected by a lock.
  embedder::ScopedPlatformHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(RawSharedBuffer);
};

// An implementation of |embedder::PlatformSharedBufferMapping|, produced by
// |RawSharedBuffer|.
class MOJO_SYSTEM_IMPL_EXPORT RawSharedBufferMapping
    : public embedder::PlatformSharedBufferMapping {
 public:
  virtual ~RawSharedBufferMapping();

  virtual void* GetBase() const OVERRIDE;
  virtual size_t GetLength() const OVERRIDE;

 private:
  friend class RawSharedBuffer;

  RawSharedBufferMapping(void* base,
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

  DISALLOW_COPY_AND_ASSIGN(RawSharedBufferMapping);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_RAW_SHARED_BUFFER_H_
