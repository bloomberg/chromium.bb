// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gpu_memory_buffer_impl.h"

#include "android_webview/public/browser/draw_gl.h"
#include "base/bind.h"
#include "base/logging.h"
#include "gpu/command_buffer/client/gpu_memory_buffer.h"
#include "ui/gfx/size.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

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
  g_gl_draw_functions->release_graphic_buffer(buffer_id_);
}

void GpuMemoryBufferImpl::Map(gpu::GpuMemoryBuffer::AccessMode mode,
    void** vaddr) {
  DCHECK(buffer_id_ != 0);
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
  DCHECK(err == 0);
  mapped_ = true;
}

void GpuMemoryBufferImpl::Unmap() {
  DCHECK(buffer_id_ != 0);
  int err = g_gl_draw_functions->unmap(buffer_id_);
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

bool GpuMemoryBufferImpl::InitCheck() {
  return buffer_id_ != 0;
}

// static
scoped_ptr<gpu::GpuMemoryBuffer> GpuMemoryBufferImpl::CreateGpuMemoryBuffer(
    int width, int height) {
  DCHECK(width > 0);
  DCHECK(height > 0);
  scoped_ptr<GpuMemoryBufferImpl> result(new GpuMemoryBufferImpl(
      gfx::Size(width, height)));

  // Check if the buffer allocation succeeded.
  if (!result->InitCheck())
    return scoped_ptr<GpuMemoryBuffer>();

  return result.PassAs<gpu::GpuMemoryBuffer>();
}

// static
void GpuMemoryBufferImpl::SetAwDrawGLFunctionTable(
    AwDrawGLFunctionTable* table) {
  g_gl_draw_functions = table;
  ::webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl::
      SetGpuMemoryBufferCreator(&CreateGpuMemoryBuffer);
}

}  // namespace android_webview
