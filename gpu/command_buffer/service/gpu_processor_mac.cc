// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/gl/gl_context.h"
#include "gpu/command_buffer/service/gpu_processor.h"

using ::base::SharedMemory;

namespace gpu {

bool GPUProcessor::Initialize(
    gfx::PluginWindowHandle window,
    const gfx::Size& size,
    const gles2::DisallowedExtensions& disallowed_extensions,
    const char* allowed_extensions,
    const std::vector<int32>& attribs,
    GPUProcessor* parent,
    uint32 parent_texture_id) {
  // Get the parent decoder and the GLContext to share IDs with, if any.
  gles2::GLES2Decoder* parent_decoder = NULL;
  gfx::GLContext* parent_context = NULL;
  if (parent) {
    parent_decoder = parent->decoder_.get();
    DCHECK(parent_decoder);

    parent_context = parent_decoder->GetGLContext();
    DCHECK(parent_context);
  }

  scoped_ptr<gfx::GLContext> context(
      gfx::GLContext::CreateOffscreenGLContext(parent_context));
  if (!context.get())
    return false;

  // On Mac OS X since we can not render on-screen we don't even
  // attempt to create a view based GLContext. The only difference
  // between "on-screen" and "off-screen" rendering on this platform
  // is whether we allocate an AcceleratedSurface, which transmits the
  // rendering results back to the browser.
  if (window) {
    surface_.reset(new AcceleratedSurface());

    // Note that although the GLContext is passed to Initialize and the
    // GLContext will later be owned by the decoder, AcceleratedSurface does
    // not hold on to the reference. It simply extracts the underlying GL
    // context in order to share the namespace with another context.
    if (!surface_->Initialize(context.get(), false)) {
      LOG(ERROR) << "GPUProcessor::Initialize failed to "
                 << "initialize AcceleratedSurface.";
      Destroy();
      return false;
    }
  }

  return InitializeCommon(context.release(),
                          size,
                          disallowed_extensions,
                          allowed_extensions,
                          attribs,
                          parent_decoder,
                          parent_texture_id);
}

void GPUProcessor::Destroy() {
  if (surface_.get()) {
    surface_->Destroy();
    surface_.reset();
  }

  DestroyCommon();
}

uint64 GPUProcessor::SetWindowSizeForIOSurface(const gfx::Size& size) {
  // This is called from an IPC handler, so it's undefined which context is
  // current. Make sure the right one is.
  decoder_->GetGLContext()->MakeCurrent();

  ResizeOffscreenFrameBuffer(size);
  decoder_->UpdateOffscreenFrameBufferSize();

  // Note: The following line changes the current context again.
  return surface_->SetSurfaceSize(size);
}

TransportDIB::Handle GPUProcessor::SetWindowSizeForTransportDIB(
    const gfx::Size& size) {
  ResizeOffscreenFrameBuffer(size);
  decoder_->UpdateOffscreenFrameBufferSize();
  return surface_->SetTransportDIBSize(size);
}

void GPUProcessor::SetTransportDIBAllocAndFree(
      Callback2<size_t, TransportDIB::Handle*>::Type* allocator,
      Callback1<TransportDIB::Id>::Type* deallocator) {
  surface_->SetTransportDIBAllocAndFree(allocator, deallocator);
}

uint64 GPUProcessor::GetSurfaceId() {
  if (!surface_.get())
    return 0;
  return surface_->GetSurfaceId();
}

uint64 GPUProcessor::swap_buffers_count() const {
  return swap_buffers_count_;
}

void GPUProcessor::set_acknowledged_swap_buffers_count(
    uint64 acknowledged_swap_buffers_count) {
  acknowledged_swap_buffers_count_ = acknowledged_swap_buffers_count;
}

void GPUProcessor::DidDestroySurface() {
  // When a browser window with a GPUProcessor is closed, the render process
  // will attempt to finish all GL commands, it will busy-wait on the GPU
  // process until the command queue is empty. If a paint is pending, the GPU
  // process won't process any GL commands until the browser sends a paint ack,
  // but since the browser window is already closed, it will never arrive.
  // To break this infinite loop, the browser tells the GPU process that the
  // surface became invalid, which causes the GPU process to not wait for paint
  // acks.
  surface_.reset();
}

void GPUProcessor::WillSwapBuffers() {
  DCHECK(decoder_.get());
  DCHECK(decoder_->GetGLContext());
  DCHECK(decoder_->GetGLContext()->IsCurrent());

  ++swap_buffers_count_;

  if (surface_.get()) {
    surface_->SwapBuffers();
  }

  if (wrapped_swap_buffers_callback_.get()) {
    wrapped_swap_buffers_callback_->Run();
  }
}

}  // namespace gpu
