// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "content/common/gpu/image_transport_surface.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/win/windows_version.h"
#include "content/common/gpu/gpu_messages.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/gfx/native_widget_types.h"

namespace {

// We are backed by an Pbuffer offscreen surface through which ANGLE provides
// a handle to the corresponding render target texture through an extension.
class PbufferImageTransportSurface
    : public gfx::GLSurfaceAdapter,
      public ImageTransportSurface,
      public base::SupportsWeakPtr<PbufferImageTransportSurface> {
 public:
  PbufferImageTransportSurface(GpuChannelManager* manager,
                               int32 render_view_id,
                               int32 renderer_id,
                               int32 command_buffer_id);

  // GLSurface implementation
  virtual bool Initialize();
  virtual void Destroy();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();

 protected:
  // ImageTransportSurface implementation
  virtual void OnNewSurfaceACK(uint64 surface_id,
                               TransportDIB::Handle shm_handle) OVERRIDE;
  virtual void OnBuffersSwappedACK() OVERRIDE;
  virtual void OnPostSubBufferACK() OVERRIDE;
  virtual void OnResizeViewACK() OVERRIDE;
  virtual void OnResize(gfx::Size size) OVERRIDE;

 private:
  virtual ~PbufferImageTransportSurface();
  void SendBuffersSwapped();

  scoped_ptr<ImageTransportHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(PbufferImageTransportSurface);
};

PbufferImageTransportSurface::PbufferImageTransportSurface(
    GpuChannelManager* manager,
    int32 render_view_id,
    int32 renderer_id,
    int32 command_buffer_id)
        : GLSurfaceAdapter(new gfx::PbufferGLSurfaceEGL(false,
                                                        gfx::Size(1, 1))) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         render_view_id,
                                         renderer_id,
                                         command_buffer_id,
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

bool PbufferImageTransportSurface::IsOffscreen() {
  return false;
}

bool PbufferImageTransportSurface::SwapBuffers() {
  HANDLE surface_handle = GetShareHandle();
  if (!surface_handle)
    return false;

  helper_->DeferToFence(base::Bind(
      &PbufferImageTransportSurface::SendBuffersSwapped,
      AsWeakPtr()));

  return true;
}

void PbufferImageTransportSurface::SendBuffersSwapped() {
  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_id = reinterpret_cast<int64>(GetShareHandle());
  params.size = GetSize();
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  helper_->SetScheduled(false);
}

void PbufferImageTransportSurface::OnBuffersSwappedACK() {
  helper_->SetScheduled(true);
}

void PbufferImageTransportSurface::OnPostSubBufferACK() {
  NOTREACHED();
}

void PbufferImageTransportSurface::OnNewSurfaceACK(
    uint64 surface_id,
    TransportDIB::Handle shm_handle) {
  NOTREACHED();
}

void PbufferImageTransportSurface::OnResizeViewACK() {
  NOTREACHED();
}

void PbufferImageTransportSurface::OnResize(gfx::Size size) {
  Resize(size);
}

}  // namespace anonymous

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateSurface(
    GpuChannelManager* manager,
    int32 render_view_id,
    int32 renderer_id,
    int32 command_buffer_id,
    gfx::PluginWindowHandle handle) {
  scoped_refptr<gfx::GLSurface> surface;

  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();

  // TODO(apatrick): Enable this once it has settled in the tree.
  if (false && gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2 &&
      os_info->version() >= base::win::VERSION_VISTA) {
    surface = new PbufferImageTransportSurface(manager,
                                               render_view_id,
                                               renderer_id,
                                               command_buffer_id);
  } else {
    surface = gfx::GLSurface::CreateViewGLSurface(false, handle);
    if (!surface.get())
      return NULL;

    surface = new PassThroughImageTransportSurface(manager,
                                                   render_view_id,
                                                   renderer_id,
                                                   command_buffer_id,
                                                   surface.get());
  }

  if (surface->Initialize())
    return surface;
  else
    return NULL;
}

#endif  // ENABLE_GPU
