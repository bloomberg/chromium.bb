// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/win/windows_version.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/common/content_switches.h"
#include "content/common/gpu/texture_image_transport_surface.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"

namespace content {
namespace {

// We are backed by an Pbuffer offscreen surface through which ANGLE provides
// a handle to the corresponding render target texture through an extension.
class PbufferImageTransportSurface
    : public gfx::GLSurfaceAdapter,
      public ImageTransportSurface,
      public base::SupportsWeakPtr<PbufferImageTransportSurface> {
 public:
  PbufferImageTransportSurface(GpuChannelManager* manager,
                               GpuCommandBufferStub* stub);

  // gfx::GLSurface implementation
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool DeferDraws() OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual bool PostSubBuffer(int x, int y, int width, int height) OVERRIDE;
  virtual std::string GetExtensions() OVERRIDE;
  virtual void SetBackbufferAllocation(bool allocated) OVERRIDE;
  virtual void SetFrontbufferAllocation(bool allocated) OVERRIDE;

 protected:
  // ImageTransportSurface implementation
  virtual void OnBufferPresented(
      const AcceleratedSurfaceMsg_BufferPresented_Params& params) OVERRIDE;
  virtual void OnResizeViewACK() OVERRIDE;
  virtual void OnResize(gfx::Size size) OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;

 private:
  virtual ~PbufferImageTransportSurface();
  void SendBuffersSwapped();
  void DestroySurface();

  // Tracks the current buffer allocation state.
  bool backbuffer_suggested_allocation_;
  bool frontbuffer_suggested_allocation_;

  // Whether a SwapBuffers is pending.
  bool is_swap_buffers_pending_;

  // Whether we unscheduled command buffer because of pending SwapBuffers.
  bool did_unschedule_;

  // Size to resize to when the surface becomes visible.
  gfx::Size visible_size_;

  scoped_ptr<ImageTransportHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(PbufferImageTransportSurface);
};

PbufferImageTransportSurface::PbufferImageTransportSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub)
    : GLSurfaceAdapter(new gfx::PbufferGLSurfaceEGL(false, gfx::Size(1, 1))),
      backbuffer_suggested_allocation_(true),
      frontbuffer_suggested_allocation_(true),
      is_swap_buffers_pending_(false),
      did_unschedule_(false) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         stub,
                                         gfx::kNullPluginWindow));
}

PbufferImageTransportSurface::~PbufferImageTransportSurface() {
  Destroy();
}

bool PbufferImageTransportSurface::Initialize() {
  // Only support this path if the GL implementation is ANGLE.
  // IO surfaces will not work with, for example, OSMesa software renderer
  // GL contexts.
  if (gfx::GetGLImplementation() != gfx::kGLImplementationEGLGLES2)
    return false;

  if (!helper_->Initialize())
    return false;

  return GLSurfaceAdapter::Initialize();
}

void PbufferImageTransportSurface::Destroy() {
  helper_->Destroy();
  GLSurfaceAdapter::Destroy();
}

bool PbufferImageTransportSurface::DeferDraws() {
  // The command buffer hit a draw/clear command that could clobber the
  // IOSurface in use by an earlier SwapBuffers. If a Swap is pending, abort
  // processing of the command by returning true and unschedule until the Swap
  // Ack arrives.
  if (did_unschedule_)
    return true;
  if (is_swap_buffers_pending_) {
    did_unschedule_ = true;
    helper_->SetScheduled(false);
    return true;
  }
  return false;
}

bool PbufferImageTransportSurface::IsOffscreen() {
  return false;
}

bool PbufferImageTransportSurface::SwapBuffers() {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_)
    return true;

  HANDLE surface_handle = GetShareHandle();
  if (!surface_handle)
    return false;

  // Don't send the surface to the browser until we hit the fence that
  // indicates the drawing to the surface has been completed.
  // TODO(jbates) unscheduling should be deferred until draw commands from the
  // next frame -- otherwise the GPU is potentially sitting idle.
  helper_->DeferToFence(base::Bind(
      &PbufferImageTransportSurface::SendBuffersSwapped,
      AsWeakPtr()));

  return true;
}

bool PbufferImageTransportSurface::PostSubBuffer(
    int x, int y, int width, int height) {
  NOTREACHED();
  return false;
}

void PbufferImageTransportSurface::SetBackbufferAllocation(bool allocation) {
  if (backbuffer_suggested_allocation_ == allocation)
    return;
  backbuffer_suggested_allocation_ = allocation;

  if (backbuffer_suggested_allocation_)
    Resize(visible_size_);
  else
    Resize(gfx::Size(1, 1));
  DestroySurface();
}

void PbufferImageTransportSurface::SetFrontbufferAllocation(bool allocation) {
  if (frontbuffer_suggested_allocation_ == allocation)
    return;
  frontbuffer_suggested_allocation_ = allocation;

  // We recreate frontbuffer by recreating backbuffer and swapping.
  // But we release frontbuffer by telling UI to release its handle on it.
  if (!frontbuffer_suggested_allocation_)
    helper_->Suspend();
}

void PbufferImageTransportSurface::DestroySurface() {
  GpuHostMsg_AcceleratedSurfaceRelease_Params params;
  helper_->SendAcceleratedSurfaceRelease(params);
}

std::string PbufferImageTransportSurface::GetExtensions() {
  std::string extensions = gfx::GLSurface::GetExtensions();
  extensions += extensions.empty() ? "" : " ";
  extensions += "GL_CHROMIUM_front_buffer_cached";
  return extensions;
}

void PbufferImageTransportSurface::SendBuffersSwapped() {
  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_handle = reinterpret_cast<int64>(GetShareHandle());
  CHECK(params.surface_handle);
  params.size = GetSize();

  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  DCHECK(!is_swap_buffers_pending_);
  is_swap_buffers_pending_ = true;
}

void PbufferImageTransportSurface::OnBufferPresented(
    const AcceleratedSurfaceMsg_BufferPresented_Params& /* params */) {
  is_swap_buffers_pending_ = false;
  if (did_unschedule_) {
    did_unschedule_ = false;
    helper_->SetScheduled(true);
  }
}

void PbufferImageTransportSurface::OnResizeViewACK() {
  NOTREACHED();
}

void PbufferImageTransportSurface::OnResize(gfx::Size size) {
  DCHECK(backbuffer_suggested_allocation_);
  DCHECK(frontbuffer_suggested_allocation_);
  Resize(size);

  DestroySurface();

  visible_size_ = size;
}

gfx::Size PbufferImageTransportSurface::GetSize() {
  return GLSurfaceAdapter::GetSize();
}

}  // namespace anonymous

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle) {
  scoped_refptr<gfx::GLSurface> surface;

  if (!handle.handle) {
    // If we don't have a valid handle with the transport flag set, then we're
    // coming from a renderer and we want to render the webpage contents to a
    // texture.
    DCHECK(handle.transport);
    surface = new TextureImageTransportSurface(manager, stub, handle);
  } else {
    if (gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2 &&
        !CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableImageTransportSurface)) {
      // This path handles two different cases.
      //
      // For post-Vista regular Windows, this surface will be used for
      // renderer compositors.
      //
      // For Aura Windows, this will be the surface for the browser compositor
      // (and the renderer compositors surface's will be
      // TextureImageTransportSurface above).
      const char* extensions = eglQueryString(
          eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_EXTENSIONS);
      if (strstr(extensions, "EGL_ANGLE_query_surface_pointer") &&
          strstr(extensions, "EGL_ANGLE_surface_d3d_texture_2d_share_handle")) {
        surface = new PbufferImageTransportSurface(manager, stub);
      }
    }

    if (!surface.get()) {
      surface = gfx::GLSurface::CreateViewGLSurface(false, handle.handle);
      if (!surface.get())
        return NULL;

      surface = new PassThroughImageTransportSurface(manager,
                                                    stub,
                                                    surface.get(),
                                                    handle.transport);
    }
  }

  if (surface->Initialize())
    return surface;
  else
    return NULL;
}

}  // namespace content
