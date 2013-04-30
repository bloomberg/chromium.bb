// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GPU_MEMORY_BUFFER_IMPL_H_
#define ANDROID_WEBVIEW_BROWSER_GPU_MEMORY_BUFFER_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "gpu/command_buffer/client/gpu_memory_buffer.h"
#include "ui/gfx/size.h"

struct AwDrawGLFunctionTable;

namespace android_webview {

class GpuMemoryBufferImpl : public gpu::GpuMemoryBuffer {
 public:
  static void SetAwDrawGLFunctionTable(AwDrawGLFunctionTable* table);
  GpuMemoryBufferImpl(gfx::Size size);
  virtual ~GpuMemoryBufferImpl();

  // methods from GpuMemoryBuffer
  virtual void Map(gpu::GpuMemoryBuffer::AccessMode mode,
      void** vaddr) OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual bool IsMapped() OVERRIDE;
  virtual void* GetNativeBuffer() OVERRIDE;
  virtual uint32 GetStride() OVERRIDE;

 private:
  int buffer_id_;
  gfx::Size size_;
  bool mapped_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImpl);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GPU_MEMORY_BUFFER_IMPL_H_
