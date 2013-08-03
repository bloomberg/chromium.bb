// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gpu_memory_buffer_factory_impl.h"

#include "android_webview/public/browser/draw_gl.h"
#include "base/logging.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_bindings.h"

namespace android_webview {

namespace {

// Provides hardware rendering functions from the Android glue layer.
AwDrawGLFunctionTable* g_gl_draw_functions = NULL;

class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImpl(int buffer_id, gfx::Size size)
      : buffer_id_(buffer_id),
        size_(size),
        mapped_(false) {
    DCHECK(buffer_id_);
  }

  virtual ~GpuMemoryBufferImpl() {
    g_gl_draw_functions->release_graphic_buffer(buffer_id_);
  }

  // Overridden from gfx::GpuMemoryBuffer:
  virtual void Map(gfx::GpuMemoryBuffer::AccessMode mode,
                   void** vaddr) OVERRIDE {
    AwMapMode map_mode = MAP_READ_ONLY;
    switch (mode) {
      case GpuMemoryBuffer::READ_ONLY:
        map_mode = MAP_READ_ONLY;
        break;
      case GpuMemoryBuffer::WRITE_ONLY:
        map_mode = MAP_WRITE_ONLY;
        break;
      case GpuMemoryBuffer::READ_WRITE:
        map_mode = MAP_READ_WRITE;
        break;
      default:
        LOG(DFATAL) << "Unknown map mode: " << mode;
    }
    int err = g_gl_draw_functions->map(buffer_id_, map_mode, vaddr);
    DCHECK(!err);
    mapped_ = true;
  }
  virtual void Unmap() OVERRIDE {
    int err = g_gl_draw_functions->unmap(buffer_id_);
    DCHECK(!err);
    mapped_ = false;
  }
  virtual bool IsMapped() const OVERRIDE { return mapped_; }
  virtual uint32 GetStride() const OVERRIDE {
    return g_gl_draw_functions->get_stride(buffer_id_);
  }
  virtual gfx::GpuMemoryBufferHandle GetHandle() const OVERRIDE {
    gfx::GpuMemoryBufferHandle handle;
    handle.type = gfx::EGL_CLIENT_BUFFER;
    handle.native_buffer = g_gl_draw_functions->get_native_buffer(buffer_id_);
    return handle;
  }

 private:
  int buffer_id_;
  gfx::Size size_;
  bool mapped_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImpl);
};

}  // namespace

GpuMemoryBufferFactoryImpl::GpuMemoryBufferFactoryImpl() {
}

GpuMemoryBufferFactoryImpl::~GpuMemoryBufferFactoryImpl() {
}

gfx::GpuMemoryBuffer* GpuMemoryBufferFactoryImpl::CreateGpuMemoryBuffer(
    size_t width,
    size_t height,
    unsigned internalformat) {
  // For Android WebView we assume the |internalformat| will always be
  // GL_RGBA8_OES.
  CHECK_EQ(static_cast<GLenum>(GL_RGBA8_OES), internalformat);
  CHECK(g_gl_draw_functions);
  int buffer_id = g_gl_draw_functions->create_graphic_buffer(width, height);
  if (!buffer_id)
    return NULL;

  return new GpuMemoryBufferImpl(buffer_id, gfx::Size(width, height));
}

// static
void GpuMemoryBufferFactoryImpl::SetAwDrawGLFunctionTable(
    AwDrawGLFunctionTable* table) {
  g_gl_draw_functions = table;
}

}  // namespace android_webview
