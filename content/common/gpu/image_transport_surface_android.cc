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
#include "content/common/gpu/null_transport_surface.h"
#include "content/public/common/content_switches.h"
#include "ui/gl/gl_surface_egl.h"

namespace content {
namespace {

// Amount of time the GPU is allowed to idle before it powers down.
const int kMaxGpuIdleTimeMs = 40;
// Maximum amount of time we keep pinging the GPU waiting for the client to
// draw.
const int kMaxKeepAliveTimeMs = 200;
// Last time we know the GPU was powered on. Global for tracking across all
// transport surfaces.
int64 g_last_gpu_access_ticks;

void DidAccessGpu() {
  g_last_gpu_access_ticks = base::TimeTicks::Now().ToInternalValue();
}

class ImageTransportSurfaceAndroid
    : public NullTransportSurface,
      public base::SupportsWeakPtr<ImageTransportSurfaceAndroid> {
 public:
  ImageTransportSurfaceAndroid(GpuChannelManager* manager,
                               GpuCommandBufferStub* stub,
                               const gfx::GLSurfaceHandle& handle);

  // gfx::GLSurface implementation.
  bool OnMakeCurrent(gfx::GLContext* context) override;
  void WakeUpGpu() override;

 protected:
  ~ImageTransportSurfaceAndroid() override;

 private:
  void ScheduleWakeUp();
  void DoWakeUpGpu();

  base::TimeTicks begin_wake_up_time_;
};

class DirectSurfaceAndroid : public PassThroughImageTransportSurface {
 public:
  DirectSurfaceAndroid(GpuChannelManager* manager,
                       GpuCommandBufferStub* stub,
                       gfx::GLSurface* surface);

  // gfx::GLSurface implementation.
  bool SwapBuffers() override;

 protected:
  ~DirectSurfaceAndroid() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DirectSurfaceAndroid);
};

ImageTransportSurfaceAndroid::ImageTransportSurfaceAndroid(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle)
    : NullTransportSurface(manager, stub, handle) {}

ImageTransportSurfaceAndroid::~ImageTransportSurfaceAndroid() {}

bool ImageTransportSurfaceAndroid::OnMakeCurrent(gfx::GLContext* context) {
  DidAccessGpu();
  return true;
}

void ImageTransportSurfaceAndroid::WakeUpGpu() {
  begin_wake_up_time_ = base::TimeTicks::Now();
  ScheduleWakeUp();
}

void ImageTransportSurfaceAndroid::ScheduleWakeUp() {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks last_access_time =
      base::TimeTicks::FromInternalValue(g_last_gpu_access_ticks);
  TRACE_EVENT2("gpu", "ImageTransportSurfaceAndroid::ScheduleWakeUp",
               "idle_time", (now - last_access_time).InMilliseconds(),
               "keep_awake_time", (now - begin_wake_up_time_).InMilliseconds());
  if (now - last_access_time <
      base::TimeDelta::FromMilliseconds(kMaxGpuIdleTimeMs))
    return;
  if (now - begin_wake_up_time_ >
      base::TimeDelta::FromMilliseconds(kMaxKeepAliveTimeMs))
    return;

  DoWakeUpGpu();

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ImageTransportSurfaceAndroid::ScheduleWakeUp, AsWeakPtr()),
      base::TimeDelta::FromMilliseconds(kMaxGpuIdleTimeMs));
}

void ImageTransportSurfaceAndroid::DoWakeUpGpu() {
  if (!GetHelper()->stub()->decoder() ||
      !GetHelper()->stub()->decoder()->MakeCurrent())
    return;
  glFinish();
  DidAccessGpu();
}

DirectSurfaceAndroid::DirectSurfaceAndroid(GpuChannelManager* manager,
                                           GpuCommandBufferStub* stub,
                                           gfx::GLSurface* surface)
    : PassThroughImageTransportSurface(manager, stub, surface) {}

DirectSurfaceAndroid::~DirectSurfaceAndroid() {}

bool DirectSurfaceAndroid::SwapBuffers() {
  DidAccessGpu();
  return PassThroughImageTransportSurface::SwapBuffers();
}

}  // anonymous namespace

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateTransportSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle) {
  DCHECK_EQ(gfx::NULL_TRANSPORT, handle.transport_type);
  return scoped_refptr<gfx::GLSurface>(
      new ImageTransportSurfaceAndroid(manager, stub, handle));
}

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateNativeSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle) {
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

  return scoped_refptr<gfx::GLSurface>(
      new DirectSurfaceAndroid(manager, stub, surface.get()));
}

}  // namespace content
