// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_surface_lookup.h"
#include "content/common/gpu/image_transport_surface.h"
#include "content/public/common/content_switches.h"
#include "ui/gl/gl_surface_egl.h"

namespace content {

namespace {

class ImageTransportSurfaceAndroid : public PassThroughImageTransportSurface {
 public:
  ImageTransportSurfaceAndroid(GpuChannelManager* manager,
                               GpuCommandBufferStub* stub,
                               gfx::GLSurface* surface,
                               uint32 parent_client_id);

  // gfx::GLSurface implementation.
  virtual bool Initialize() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual std::string GetExtensions() OVERRIDE;
  virtual void SetFrontbufferAllocation(bool allocated) OVERRIDE;

 protected:
  virtual ~ImageTransportSurfaceAndroid();

 private:
  uint32 parent_client_id_;
  bool frontbuffer_suggested_allocation_;
};

ImageTransportSurfaceAndroid::ImageTransportSurfaceAndroid(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    gfx::GLSurface* surface,
    uint32 parent_client_id)
    : PassThroughImageTransportSurface(manager, stub, surface, true),
      parent_client_id_(parent_client_id),
      frontbuffer_suggested_allocation_(true) {}

ImageTransportSurfaceAndroid::~ImageTransportSurfaceAndroid() {}

bool ImageTransportSurfaceAndroid::Initialize() {
  if (!surface())
    return false;

  if (!PassThroughImageTransportSurface::Initialize())
    return false;

  GpuChannel* parent_channel =
      GetHelper()->manager()->LookupChannel(parent_client_id_);
  if (parent_channel) {
    const CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kUIPrioritizeInGpuProcess))
      GetHelper()->SetPreemptByFlag(parent_channel->GetPreemptionFlag());
  }

  return true;
}

std::string ImageTransportSurfaceAndroid::GetExtensions() {
  std::string extensions = gfx::GLSurface::GetExtensions();
  extensions += extensions.empty() ? "" : " ";
  extensions += "GL_CHROMIUM_front_buffer_cached ";
  return extensions;
}

void ImageTransportSurfaceAndroid::SetFrontbufferAllocation(bool allocation) {
  if (frontbuffer_suggested_allocation_ == allocation)
    return;
  frontbuffer_suggested_allocation_ = allocation;
  // TODO(sievers): This races with CompositorFrame messages.
  if (!allocation)
    GetHelper()->SendAcceleratedSurfaceRelease();
}

bool ImageTransportSurfaceAndroid::SwapBuffers() {
  NOTREACHED();
  return false;
}

}  // anonymous namespace

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateNativeSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle) {
  if (handle.transport_type == gfx::NATIVE_TRANSPORT) {
    return scoped_refptr<gfx::GLSurface>(
        new ImageTransportSurfaceAndroid(manager,
                                         stub,
                                         manager->GetDefaultOffscreenSurface(),
                                         handle.parent_client_id));
  }

  DCHECK(GpuSurfaceLookup::GetInstance());
  DCHECK_EQ(handle.transport_type, gfx::NATIVE_DIRECT);
  ANativeWindow* window =
      GpuSurfaceLookup::GetInstance()->AcquireNativeWidget(
          stub->surface_id());
  scoped_refptr<gfx::GLSurface> surface =
      new gfx::NativeViewGLSurfaceEGL(window);
  bool initialize_success = surface->Initialize();
  if (window)
    ANativeWindow_release(window);
  if (!initialize_success)
    return scoped_refptr<gfx::GLSurface>();

  return scoped_refptr<gfx::GLSurface>(new PassThroughImageTransportSurface(
      manager, stub, surface.get(), false));
}

}  // namespace content
