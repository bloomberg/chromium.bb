// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gpu_memory_buffer_impl.h"

#include "android_webview/browser/gpu_memory_buffer_factory_impl.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/bind.h"
#include "base/logging.h"
#include "gpu/command_buffer/client/gpu_memory_buffer.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_factory.h"
#include "ui/gfx/size.h"

namespace android_webview {

// Provides hardware rendering functions from the Android glue layer.
AwDrawGLFunctionTable* g_gl_draw_functions = NULL;

GpuMemoryBufferImpl::GpuMemoryBufferImpl(gfx::Size size)
    : buffer_id_(g_gl_draw_functions->create_graphic_buffer(
          size.width(), size.height())),
      size_(size),
      mapped_(false) {
}

GpuMemoryBufferImpl::~GpuMemoryBufferImpl() {
  DCHECK(buffer_id_ != 0);
  g_gl_draw_functions->release_graphic_buffer(buffer_id_);
  buffer_id_ = 0;
}

void GpuMemoryBufferImpl::Map(gpu::GpuMemoryBuffer::AccessMode mode,
    void** vaddr) {
  DCHECK(buffer_id_ != 0);
  int err = g_gl_draw_functions->lock(buffer_id_, mode, vaddr);
  DCHECK(err == 0);
  mapped_ = true;
}

void GpuMemoryBufferImpl::Unmap() {
  DCHECK(buffer_id_ != 0);
  int err = g_gl_draw_functions->unlock(buffer_id_);
  DCHECK(err == 0);
  mapped_ = false;
}

void* GpuMemoryBufferImpl::GetNativeBuffer() {
  DCHECK(buffer_id_ != 0);
  return g_gl_draw_functions->get_native_buffer(buffer_id_);
}

uint32 GpuMemoryBufferImpl::GetStride() {
  DCHECK(buffer_id_ != 0);
  return g_gl_draw_functions->get_stride(buffer_id_);
}

bool GpuMemoryBufferImpl::IsMapped() {
  return mapped_;
}

// static
void GpuMemoryBufferImpl::SetAwDrawGLFunctionTable(
    AwDrawGLFunctionTable* table) {
  g_gl_draw_functions = table;
  gpu::SetProcessDefaultGpuMemoryBufferFactory(
      base::Bind(&CreateGpuMemoryBuffer));
}

}  // namespace android_webview
