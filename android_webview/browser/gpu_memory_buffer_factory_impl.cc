// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gpu_memory_buffer_factory_impl.h"

#include "android_webview/public/browser/draw_gl.h"
#include "base/logging.h"
#include "gpu/command_buffer/service/in_process_command_buffer.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_android_native_buffer.h"

namespace android_webview {

namespace {

// Provides hardware rendering functions from the Android glue layer.
AwDrawGLFunctionTable* g_gl_draw_functions = NULL;

class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImpl(long buffer_id, const gfx::Size& size)
      : buffer_id_(buffer_id), size_(size), mapped_(false) {
    DCHECK(buffer_id_);
  }

  virtual ~GpuMemoryBufferImpl() {
    g_gl_draw_functions->release_graphic_buffer(buffer_id_);
  }

  // Overridden from gfx::GpuMemoryBuffer:
  virtual void* Map() OVERRIDE {
    void* vaddr = NULL;
    int err = g_gl_draw_functions->map(buffer_id_, MAP_READ_WRITE, &vaddr);
    DCHECK(!err);
    mapped_ = true;
    return vaddr;
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
    handle.type = gfx::ANDROID_NATIVE_BUFFER;
    handle.buffer_id = buffer_id_;
    return handle;
  }

 private:
  long buffer_id_;
  gfx::Size size_;
  bool mapped_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImpl);
};

}  // namespace

GpuMemoryBufferFactoryImpl::GpuMemoryBufferFactoryImpl() {
}

GpuMemoryBufferFactoryImpl::~GpuMemoryBufferFactoryImpl() {
}

scoped_ptr<gfx::GpuMemoryBuffer>
GpuMemoryBufferFactoryImpl::AllocateGpuMemoryBuffer(size_t width,
                                                    size_t height,
                                                    unsigned internalformat,
                                                    unsigned usage) {
  // For Android WebView we assume the |internalformat| will always be
  // GL_RGBA8_OES.
  CHECK_EQ(static_cast<GLenum>(GL_RGBA8_OES), internalformat);
  CHECK(g_gl_draw_functions);
  long buffer_id = g_gl_draw_functions->create_graphic_buffer(width, height);
  if (!buffer_id)
    return scoped_ptr<gfx::GpuMemoryBuffer>();

  return make_scoped_ptr(
             new GpuMemoryBufferImpl(buffer_id, gfx::Size(width, height)))
      .PassAs<gfx::GpuMemoryBuffer>();
}

scoped_refptr<gfx::GLImage>
GpuMemoryBufferFactoryImpl::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    unsigned internalformat) {
  DCHECK_EQ(gfx::ANDROID_NATIVE_BUFFER, handle.type);

  EGLClientBuffer native_buffer =
      g_gl_draw_functions->get_native_buffer(handle.buffer_id);
  DCHECK(native_buffer);

  scoped_refptr<gfx::GLImageAndroidNativeBuffer> image(
      new gfx::GLImageAndroidNativeBuffer(size));
  if (!image->Initialize(native_buffer))
    return scoped_refptr<gfx::GLImage>();

  return image;
}

// static
void GpuMemoryBufferFactoryImpl::SetAwDrawGLFunctionTable(
    AwDrawGLFunctionTable* table) {
  g_gl_draw_functions = table;
}

bool GpuMemoryBufferFactoryImpl::Initialize() {
  if (!g_gl_draw_functions)
    return false;

  gpu::InProcessCommandBuffer::SetGpuMemoryBufferFactory(this);
  return true;
}

}  // namespace android_webview
