// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_RAW_SHARED_BUFFER_H_
#define MOJO_SYSTEM_RAW_SHARED_BUFFER_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "mojo/embedder/scoped_platform_handle.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

// |RawSharedBuffer| is a thread-safe, ref-counted wrapper around OS-specific
// shared memory. It has the following features:
//   - A |RawSharedBuffer| simply represents a piece of shared memory that *may*
//     be mapped and *may* be shared to another process.
//   - A single |RawSharedBuffer| may be mapped multiple times. The lifetime of
//     the mapping (owned by |RawSharedBuffer::Mapping|) is separate from the
//     lifetime of the |RawSharedBuffer|.
//   - Sizes/offsets (of the shared memory and mappings) are arbitrary, and not
//     restricted by page size. However, more memory may actually be mapped than
//     requested.
//
// It currently does NOT support the following:
//   - Sharing read-only. (This will probably eventually be supported.)
//
// TODO(vtl): Rectify this with |base::SharedMemory|.
class MOJO_SYSTEM_IMPL_EXPORT RawSharedBuffer
    : public base::RefCountedThreadSafe<RawSharedBuffer> {
 public:
  // A mapping of a |RawSharedBuffer| (compararable to a "file view" in
  // Windows); see above. Created by |RawSharedBuffer::Map()|. Automatically
  // unmaps memory on destruction.
  //
  // Mappings are NOT thread-safe.
  class MOJO_SYSTEM_IMPL_EXPORT Mapping {
   public:
    ~Mapping() { Unmap(); }

    void* base() const { return base_; }
    size_t length() const { return length_; }

   private:
    friend class RawSharedBuffer;

    Mapping(void* base, size_t length, void* real_base, size_t real_length)
        : base_(base), length_(length),
          real_base_(real_base), real_length_(real_length) {}
    void Unmap();

    void* const base_;
    const size_t length_;

    void* const real_base_;
    const size_t real_length_;

    DISALLOW_COPY_AND_ASSIGN(Mapping);
  };

  // Creates a shared buffer of size |num_bytes| bytes (initially zero-filled).
  // |num_bytes| must be nonzero. Returns null on failure.
  static RawSharedBuffer* Create(size_t num_bytes);

  // Maps (some) of the shared buffer into memory; [|offset|, |offset + length|]
  // must be contained in [0, |num_bytes|], and |length| must be at least 1.
  // Returns null on failure.
  scoped_ptr<Mapping> Map(size_t offset, size_t length);

  // Checks if |offset| and |length| are valid arguments.
  bool IsValidMap(size_t offset, size_t length);

  // Like |Map()|, but doesn't check its arguments (which should have been
  // preflighted using |IsValidMap()|).
  scoped_ptr<Mapping> MapNoCheck(size_t offset, size_t length);

  size_t num_bytes() const { return num_bytes_; }

 private:
  friend class base::RefCountedThreadSafe<RawSharedBuffer>;

  explicit RawSharedBuffer(size_t num_bytes);
  ~RawSharedBuffer();

  // This is called by |Create()| before this object is given to anyone (hence
  // it doesn't need to take |lock_|).
  bool InitNoLock();

  // The platform-dependent part of |Map()|; doesn't check arguments. Called
  // under |lock_|.
  scoped_ptr<Mapping> MapImplNoLock(size_t offset, size_t length);

  const size_t num_bytes_;

  base::Lock lock_;  // Protects |handle_|.
  embedder::ScopedPlatformHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(RawSharedBuffer);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_RAW_SHARED_BUFFER_H_
