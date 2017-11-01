// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_PROTECTED_BUFFER_MANAGER_H_
#define CHROME_GPU_PROTECTED_BUFFER_MANAGER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_pixmap.h"

namespace chromeos {
namespace arc {

// A ProtectedBufferHandle is returned to the owning client that requested
// the underlying ProtectedBuffer to be allocated.
//
// A ProtectedBuffer is a buffer that can be referred to via a handle (a dummy
// handle), which does not provide access to the actual contents of the buffer.
//
// The client should release this handle once the buffer is no longer needed.
// Releasing triggers destruction of the ProtectedBuffer instance stored in
// the ProtectedBufferManager, via the destruction callback passed to the
// ProtectedBufferHandle's constructor.
class ProtectedBufferHandle {
 public:
  // ProtectedBufferHandle takes ownership of the passed |shm_handle|.
  ProtectedBufferHandle(base::OnceClosure destruction_cb,
                        const base::SharedMemoryHandle& shm_handle);

  // ProtectedBufferHandle takes ownership of the passed |native_pixmap_handle|.
  ProtectedBufferHandle(base::OnceClosure destruction_cb,
                        const gfx::NativePixmapHandle& native_pixmap_handle);

  // Closes the underlying handle.
  ~ProtectedBufferHandle();

  // Return a non-owned SharedMemoryHandle or NativePixmapHandle for this
  // ProtectedBufferHandle, or an invalid/null handle if not applicable for the
  // underlying type.
  base::SharedMemoryHandle shm_handle() const;
  gfx::NativePixmapHandle native_pixmap_handle() const;

 private:
  // The underlying, owning handles to the protected buffer.
  // Only one of the handles is valid for each instance of this class.
  // Closed on destruction of this ProtectedBufferHandle.
  base::SharedMemoryHandle shm_handle_;
  gfx::NativePixmapHandle native_pixmap_handle_;

  base::OnceClosure destruction_cb_;
};

class ProtectedBufferManager {
 public:
  ProtectedBufferManager();
  ~ProtectedBufferManager();

  // Allocate a ProtectedSharedMemory buffer of |size| bytes, to be referred to
  // via |dummy_fd| as the dummy handle, returning a ProtectedBufferHandle to
  // it.
  // Destroying the ProtectedBufferHandle will result in permanently
  // disassociating the |dummy_fd| with the underlying ProtectedBuffer, but may
  // not free the underlying protected memory, which will remain valid as long
  // as any SharedMemoryHandles to it are still in use.
  // Return nullptr on failure.
  std::unique_ptr<ProtectedBufferHandle> AllocateProtectedSharedMemory(
      base::ScopedFD dummy_fd,
      size_t size);

  // Allocate a ProtectedNativePixmap of |format| and |size|, to be referred to
  // via |dummy_fd| as the dummy handle, returning a ProtectedBufferHandle to
  // it.
  // Destroying the ProtectedBufferHandle will result in permanently
  // disassociating the |dummy_fd| with the underlying ProtectedBuffer, but may
  // not free the underlying protected memory, which will remain valid as long
  // as any NativePixmapHandles to it are still in use.
  // Return nullptr on failure.
  std::unique_ptr<ProtectedBufferHandle> AllocateProtectedNativePixmap(
      base::ScopedFD dummy_fd,
      gfx::BufferFormat format,
      const gfx::Size& size);

  // Return a duplicated SharedMemoryHandle associated with the |dummy_fd|,
  // if one exists, or an invalid handle otherwise.
  // The client is responsible for closing the handle after use.
  base::SharedMemoryHandle GetProtectedSharedMemoryHandleFor(
      base::ScopedFD dummy_fd);

  // Return a duplicated NativePixmapHandle associated with the |dummy_fd|,
  // if one exists, or an empty handle otherwise.
  // The client is responsible for closing the handle after use.
  gfx::NativePixmapHandle GetProtectedNativePixmapHandleFor(
      base::ScopedFD dummy_fd);

  // Return a protected NativePixmap for a dummy |handle|, if one exists, or
  // nullptr otherwise. On success, the |handle| is closed.
  scoped_refptr<gfx::NativePixmap> GetProtectedNativePixmapFor(
      const gfx::NativePixmapHandle& handle);

 private:
  // Used internally to maintain the association between the dummy handle and
  // the underlying buffer.
  class ProtectedBuffer;
  class ProtectedSharedMemory;
  class ProtectedNativePixmap;

  // Imports the |dummy_fd| as a NativePixmap. This returns a unique |id|,
  // which is guaranteed to be the same for all future imports of any fd
  // referring to the buffer to which |dummy_fd| refers to, regardless of
  // whether it is the same fd as the original one, or not, for the lifetime
  // of the buffer.
  //
  // This allows us to have an unambiguous mapping from any fd referring to
  // the same memory buffer to the same unique id.
  //
  // Returns nullptr on failure, in which case the returned id is not valid.
  scoped_refptr<gfx::NativePixmap> ImportDummyFd(base::ScopedFD dummy_fd,
                                                 uint32_t* id) const;

  // Removes an entry for given |id| from buffer_map_, to be called when the
  // last reference to the buffer is dropped.
  void RemoveEntry(uint32_t id);

  // A map of unique ids to the ProtectedBuffers associated with them.
  using ProtectedBufferMap =
      std::map<uint32_t, std::unique_ptr<ProtectedBuffer>>;
  ProtectedBufferMap buffer_map_;
  base::Lock buffer_map_lock_;

  base::WeakPtr<ProtectedBufferManager> weak_this_;
  base::WeakPtrFactory<ProtectedBufferManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProtectedBufferManager);
};

}  // namespace arc
}  // namespace chromeos

#endif  // CHROME_GPU_PROTECTED_BUFFER_MANAGER_H_
