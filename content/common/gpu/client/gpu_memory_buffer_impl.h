// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace content {

// Provides common implementation of a GPU memory buffer.
class CONTENT_EXPORT GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  typedef base::Callback<void(uint32 sync_point)> DestructionCallback;

  ~GpuMemoryBufferImpl() override;

  // Creates an instance from the given |handle|. |size| and |internalformat|
  // should match what was used to allocate the |handle|. |callback| is
  // called when instance is deleted, which is not necessarily on the same
  // thread as this function was called on and instance was created on.
  static scoped_ptr<GpuMemoryBufferImpl> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      const DestructionCallback& callback);

  // Type-checking upcast routine. Returns an NULL on failure.
  static GpuMemoryBufferImpl* FromClientBuffer(ClientBuffer buffer);

  // Returns the number of planes based on the format of the buffer.
  static size_t NumberOfPlanesForGpuMemoryBufferFormat(
      gfx::BufferFormat format);

  // Returns the subsampling factor applied to the given zero-indexed |plane| of
  // the |format| both horizontally and vertically.
  static size_t SubsamplingFactor(gfx::BufferFormat format, int plane);

  // Returns the number of bytes used to store a row of the given zero-indexed
  // |plane| of |format|.
  // Note: This is an approximation and the exact size used by an implementation
  // might be different.
  static bool RowSizeInBytes(size_t width,
                             gfx::BufferFormat format,
                             int plane,
                             size_t* size_in_bytes);

  // Returns the number of bytes used to store all the planes of a given
  // |format|.
  // Note: This is an approximation and the exact size used by an implementation
  // might be different.
  static bool BufferSizeInBytes(const gfx::Size& size,
                                gfx::BufferFormat format,
                                size_t* size_in_bytes);

  // Overridden from gfx::GpuMemoryBuffer:
  bool IsMapped() const override;
  gfx::BufferFormat GetFormat() const override;
  gfx::GpuMemoryBufferId GetId() const override;
  ClientBuffer AsClientBuffer() override;

  void set_destruction_sync_point(uint32 sync_point) {
    destruction_sync_point_ = sync_point;
  }

 protected:
  GpuMemoryBufferImpl(gfx::GpuMemoryBufferId id,
                      const gfx::Size& size,
                      gfx::BufferFormat format,
                      const DestructionCallback& callback);

  const gfx::GpuMemoryBufferId id_;
  const gfx::Size size_;
  const gfx::BufferFormat format_;
  const DestructionCallback callback_;
  bool mapped_;
  uint32 destruction_sync_point_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_
