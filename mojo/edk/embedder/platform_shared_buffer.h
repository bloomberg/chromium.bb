// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_PLATFORM_SHARED_BUFFER_H_
#define MOJO_EDK_EMBEDDER_PLATFORM_SHARED_BUFFER_H_

#include <stddef.h>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/shared_memory_handle.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace edk {

class PlatformSharedBufferMapping;

// |PlatformSharedBuffer| is a thread-safe, ref-counted wrapper around
// OS-specific shared memory. It has the following features:
//   - A |PlatformSharedBuffer| simply represents a piece of shared memory that
//     *may* be mapped and *may* be shared to another process.
//   - A single |PlatformSharedBuffer| may be mapped multiple times. The
//     lifetime of the mapping (owned by |PlatformSharedBufferMapping|) is
//     separate from the lifetime of the |PlatformSharedBuffer|.
//   - Sizes/offsets (of the shared memory and mappings) are arbitrary, and not
//     restricted by page size. However, more memory may actually be mapped than
//     requested.
//
// It currently does NOT support the following:
//   - Sharing read-only. (This will probably eventually be supported.)
class MOJO_SYSTEM_IMPL_EXPORT PlatformSharedBuffer
    : public base::RefCountedThreadSafe<PlatformSharedBuffer> {
 public:
  // Creates a shared buffer of size |num_bytes| bytes (initially zero-filled).
  // |num_bytes| must be nonzero. Returns null on failure.
  static PlatformSharedBuffer* Create(size_t num_bytes);

  // Creates a shared buffer of size |num_bytes| from the existing platform
  // handle |platform_handle|. Returns null on failure.
  static PlatformSharedBuffer* CreateFromPlatformHandle(
      size_t num_bytes,
      ScopedPlatformHandle platform_handle);

  // Creates a shared buffer of size |num_bytes| from the existing shared memory
  // handle |handle|.
  static PlatformSharedBuffer* CreateFromSharedMemoryHandle(
      size_t num_bytes,
      bool read_only,
      base::SharedMemoryHandle handle);

  // Gets the size of shared buffer (in number of bytes).
  size_t GetNumBytes() const;

  // Maps (some) of the shared buffer into memory; [|offset|, |offset + length|]
  // must be contained in [0, |num_bytes|], and |length| must be at least 1.
  // Returns null on failure.
  scoped_ptr<PlatformSharedBufferMapping> Map(size_t offset, size_t length);

  // Checks if |offset| and |length| are valid arguments.
  bool IsValidMap(size_t offset, size_t length);

  // Like |Map()|, but doesn't check its arguments (which should have been
  // preflighted using |IsValidMap()|).
  scoped_ptr<PlatformSharedBufferMapping> MapNoCheck(size_t offset,
                                                     size_t length);

  // Duplicates the underlying platform handle and passes it to the caller.
  // TODO(vtl): On POSIX, we'll need two FDs to support sharing read-only.
  ScopedPlatformHandle DuplicatePlatformHandle();

  // Passes the underlying platform handle to the caller. This should only be
  // called if there's a unique reference to this object (owned by the caller).
  // After calling this, this object should no longer be used, but should only
  // be disposed of.
  ScopedPlatformHandle PassPlatformHandle();

 private:
  friend class base::RefCountedThreadSafe<PlatformSharedBuffer>;

  explicit PlatformSharedBuffer(size_t num_bytes);
  ~PlatformSharedBuffer();

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

  MOJO_DISALLOW_COPY_AND_ASSIGN(PlatformSharedBuffer);
};

// A mapping of a |PlatformSharedBuffer| (compararable to a "file view" in
// Windows); see above. Created by |PlatformSharedBuffer::Map()|. Automatically
// unmaps memory on destruction.
//
// Mappings are NOT thread-safe.
//
// Note: This is an entirely separate class (instead of
// |PlatformSharedBuffer::Mapping|) so that it can be forward-declared.
class MOJO_SYSTEM_IMPL_EXPORT PlatformSharedBufferMapping {
 public:
  ~PlatformSharedBufferMapping();

  void* GetBase() const;
  size_t GetLength() const;

 private:
  friend class PlatformSharedBuffer;

  PlatformSharedBufferMapping(base::SharedMemoryHandle handle,
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

  MOJO_DISALLOW_COPY_AND_ASSIGN(PlatformSharedBufferMapping);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_PLATFORM_SHARED_BUFFER_H_
