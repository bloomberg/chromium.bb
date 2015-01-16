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
      Format format,
      const DestructionCallback& callback);

  // Type-checking upcast routine. Returns an NULL on failure.
  static GpuMemoryBufferImpl* FromClientBuffer(ClientBuffer buffer);

  // Calculates the number of bytes that an implementation must use to store
  // one row of pixel data.
  static bool StrideInBytes(size_t width,
                            Format format,
                            size_t* stride_in_bytes);

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

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_H_
