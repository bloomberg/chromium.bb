// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/size.h"

namespace content {

// Provides common implementation of a GPU memory buffer.
class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  typedef base::Callback<void(scoped_ptr<GpuMemoryBufferImpl> buffer)>
      CreationCallback;
  typedef base::Callback<void(const gfx::GpuMemoryBufferHandle& handle)>
      AllocationCallback;
  typedef base::Callback<void(uint32 sync_point)> DestructionCallback;

  ~GpuMemoryBufferImpl() override;

  // Creates a GPU memory buffer instance with |size| and |format| for |usage|
  // by the current process and |client_id|.
  static void Create(gfx::GpuMemoryBufferId id,
                     const gfx::Size& size,
                     Format format,
                     Usage usage,
                     int client_id,
                     const CreationCallback& callback);

  // Allocates a GPU memory buffer with |size| and |internalformat| for |usage|
  // by |child_process| and |child_client_id|. The |handle| returned can be
  // used by the |child_process| to create an instance of this class.
  static void AllocateForChildProcess(gfx::GpuMemoryBufferId id,
                                      const gfx::Size& size,
                                      Format format,
                                      Usage usage,
                                      base::ProcessHandle child_process,
                                      int child_client_id,
                                      const AllocationCallback& callback);

  // Notify that GPU memory buffer has been deleted by |child_process|.
  static void DeletedByChildProcess(gfx::GpuMemoryBufferType type,
                                    gfx::GpuMemoryBufferId id,
                                    base::ProcessHandle child_process,
                                    int child_client_id,
                                    uint32 sync_point);

  // Creates an instance from the given |handle|. |size| and |internalformat|
  // should match what was used to allocate the |handle|. |callback| is
  // called when instance is deleted, which is not necessarily on the same
  // thread as this function was called on and instance was created on.
  static scoped_ptr<GpuMemoryBufferImpl> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      Format format,
      const DestructionCallback& callback);

  // Type-checking upcast routine. Returns an NULL on failure.
  static GpuMemoryBufferImpl* FromClientBuffer(ClientBuffer buffer);

  // Returns the number of bytes per pixel that must be used by an
  // implementation when using |format|.
  static size_t BytesPerPixel(Format format);

  // Overridden from gfx::GpuMemoryBuffer:
  bool IsMapped() const override;
  Format GetFormat() const override;
  ClientBuffer AsClientBuffer() override;

  void set_destruction_sync_point(uint32 sync_point) {
    destruction_sync_point_ = sync_point;
  }

 protected:
  GpuMemoryBufferImpl(gfx::GpuMemoryBufferId id,
                      const gfx::Size& size,
                      Format format,
                      const DestructionCallback& callback);

  const gfx::GpuMemoryBufferId id_;
  const gfx::Size size_;
  const Format format_;
  const DestructionCallback callback_;
  bool mapped_;
  uint32 destruction_sync_point_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_
