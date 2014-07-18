// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GPU_MEMORY_BUFFER_FACTORY_IMPL_H_
#define ANDROID_WEBVIEW_BROWSER_GPU_MEMORY_BUFFER_FACTORY_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "gpu/command_buffer/service/in_process_command_buffer.h"

struct AwDrawGLFunctionTable;

namespace android_webview {

class GpuMemoryBufferFactoryImpl : public gpu::InProcessGpuMemoryBufferFactory {
 public:
  GpuMemoryBufferFactoryImpl();
  virtual ~GpuMemoryBufferFactoryImpl();

  static void SetAwDrawGLFunctionTable(AwDrawGLFunctionTable* table);

  // Overridden from gpu::InProcessGpuMemoryBufferFactory:
  virtual scoped_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBuffer(
      size_t width,
      size_t height,
      unsigned internalformat,
      unsigned usage) OVERRIDE;
  virtual scoped_refptr<gfx::GLImage> CreateImageForGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      unsigned internalformat) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferFactoryImpl);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GPU_MEMORY_BUFFER_FACTORY_IMPL_H_
