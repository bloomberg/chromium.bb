// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_share_group.h"
#include "ui/gfx/gl/gl_surface.h"

using ::base::SharedMemory;

namespace gpu {

bool GpuScheduler::Initialize(
    gfx::PluginWindowHandle window,
    const gfx::Size& size,
    bool software,
    const gles2::DisallowedExtensions& disallowed_extensions,
    const char* allowed_extensions,
    const std::vector<int32>& attribs,
    gfx::GLShareGroup* share_group) {
  scoped_refptr<gfx::GLSurface> surface(
      gfx::GLSurface::CreateOffscreenGLSurface(software, gfx::Size(1, 1)));
  if (!surface.get()) {
    LOG(ERROR) << "CreateOffscreenGLSurface failed.\n";
    Destroy();
    return false;
  }

  // Create a GLContext and attach the surface.
  scoped_refptr<gfx::GLContext> context(
      gfx::GLContext::CreateGLContext(share_group, surface.get()));
  if (!context.get()) {
    LOG(ERROR) << "CreateGLContext failed.\n";
    Destroy();
    return false;
  }

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
      LOG(ERROR) << "GpuScheduler::Initialize failed to "
                 << "initialize AcceleratedSurface.";
      Destroy();
      return false;
    }
  }

  return InitializeCommon(surface,
                          context,
                          size,
                          disallowed_extensions,
                          allowed_extensions,
                          attribs);
}

void GpuScheduler::Destroy() {
  if (surface_.get()) {
    surface_->Destroy();
    surface_.reset();
  }

  DestroyCommon();
}

uint64 GpuScheduler::SetWindowSizeForIOSurface(const gfx::Size& size) {
  // Note: The following line changes the current context again.
  return surface_->SetSurfaceSize(size);
}

TransportDIB::Handle GpuScheduler::SetWindowSizeForTransportDIB(
    const gfx::Size& size) {
  return surface_->SetTransportDIBSize(size);
}

void GpuScheduler::SetTransportDIBAllocAndFree(
      Callback2<size_t, TransportDIB::Handle*>::Type* allocator,
      Callback1<TransportDIB::Id>::Type* deallocator) {
  surface_->SetTransportDIBAllocAndFree(allocator, deallocator);
}

uint64 GpuScheduler::GetSurfaceId() {
  if (!surface_.get())
    return 0;
  return surface_->GetSurfaceId();
}

uint64 GpuScheduler::swap_buffers_count() const {
  return swap_buffers_count_;
}

uint64 GpuScheduler::acknowledged_swap_buffers_count() const {
  return acknowledged_swap_buffers_count_;
}

void GpuScheduler::set_acknowledged_swap_buffers_count(
    uint64 acknowledged_swap_buffers_count) {
  acknowledged_swap_buffers_count_ = acknowledged_swap_buffers_count;
}

void GpuScheduler::DidDestroySurface() {
  // When a browser window with a GpuScheduler is closed, the render process
  // will attempt to finish all GL commands, it will busy-wait on the GPU
  // process until the command queue is empty. If a paint is pending, the GPU
  // process won't process any GL commands until the browser sends a paint ack,
  // but since the browser window is already closed, it will never arrive.
  // To break this infinite loop, the browser tells the GPU process that the
  // surface became invalid, which causes the GPU process to not wait for paint
  // acks.
  surface_.reset();
}

void GpuScheduler::WillSwapBuffers() {
  DCHECK(decoder_.get());
  DCHECK(decoder_->GetGLContext());
  DCHECK(decoder_->GetGLContext()->IsCurrent(decoder_->GetGLSurface()));

  ++swap_buffers_count_;

  if (surface_.get()) {
    surface_->SwapBuffers();
  }

  if (wrapped_swap_buffers_callback_.get()) {
    wrapped_swap_buffers_callback_->Run();
  }
}

}  // namespace gpu
