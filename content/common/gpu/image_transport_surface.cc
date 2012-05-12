// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "content/common/gpu/image_transport_surface.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_messages.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ui/gl/gl_switches.h"

ImageTransportSurface::ImageTransportSurface() {}

ImageTransportSurface::~ImageTransportSurface() {}

void ImageTransportSurface::GetRegionsToCopy(
    const gfx::Rect& previous_damage_rect,
    const gfx::Rect& new_damage_rect,
    std::vector<gfx::Rect>* regions) {
  gfx::Rect intersection = previous_damage_rect.Intersect(new_damage_rect);

  if (intersection.IsEmpty()) {
    regions->push_back(previous_damage_rect);
    return;
  }

  // Top (above the intersection).
  regions->push_back(gfx::Rect(previous_damage_rect.x(),
      previous_damage_rect.y(),
      previous_damage_rect.width(),
      intersection.y() - previous_damage_rect.y()));

  // Left (of the intersection).
  regions->push_back(gfx::Rect(previous_damage_rect.x(),
      intersection.y(),
      intersection.x() - previous_damage_rect.x(),
      intersection.height()));

  // Right (of the intersection).
  regions->push_back(gfx::Rect(intersection.right(),
      intersection.y(),
      previous_damage_rect.right() - intersection.right(),
      intersection.height()));

  // Bottom (below the intersection).
  regions->push_back(gfx::Rect(previous_damage_rect.x(),
      intersection.bottom(),
      previous_damage_rect.width(),
      previous_damage_rect.bottom() - intersection.bottom()));
}

ImageTransportHelper::ImageTransportHelper(ImageTransportSurface* surface,
                                           GpuChannelManager* manager,
                                           GpuCommandBufferStub* stub,
                                           gfx::PluginWindowHandle handle)
    : surface_(surface),
      manager_(manager),
      stub_(stub->AsWeakPtr()),
      handle_(handle) {
  route_id_ = manager_->GenerateRouteID();
  manager_->AddRoute(route_id_, this);
}

ImageTransportHelper::~ImageTransportHelper() {
  manager_->RemoveRoute(route_id_);
}

bool ImageTransportHelper::Initialize() {
  gpu::gles2::GLES2Decoder* decoder = Decoder();

  if (!decoder)
    return false;

  decoder->SetResizeCallback(
       base::Bind(&ImageTransportHelper::Resize, base::Unretained(this)));

  return true;
}

void ImageTransportHelper::Destroy() {}

bool ImageTransportHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ImageTransportHelper, message)
    IPC_MESSAGE_HANDLER(AcceleratedSurfaceMsg_BuffersSwappedACK,
                        OnBuffersSwappedACK)
    IPC_MESSAGE_HANDLER(AcceleratedSurfaceMsg_PostSubBufferACK,
                        OnPostSubBufferACK)
    IPC_MESSAGE_HANDLER(AcceleratedSurfaceMsg_NewACK,
                        OnNewSurfaceACK)
    IPC_MESSAGE_HANDLER(AcceleratedSurfaceMsg_ResizeViewACK, OnResizeViewACK);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ImageTransportHelper::SendAcceleratedSurfaceNew(
    GpuHostMsg_AcceleratedSurfaceNew_Params params) {
  params.surface_id = stub_->surface_id();
  params.route_id = route_id_;
#if defined(OS_MACOSX)
  params.window = handle_;
#endif
  manager_->Send(new GpuHostMsg_AcceleratedSurfaceNew(params));
}

void ImageTransportHelper::SendAcceleratedSurfaceBuffersSwapped(
    GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params) {
  params.surface_id = stub_->surface_id();
  params.route_id = route_id_;
#if defined(OS_MACOSX)
  params.window = handle_;
#endif
  manager_->Send(new GpuHostMsg_AcceleratedSurfaceBuffersSwapped(params));
}

void ImageTransportHelper::SendAcceleratedSurfacePostSubBuffer(
    GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params params) {
  params.surface_id = stub_->surface_id();
  params.route_id = route_id_;
#if defined(OS_MACOSX)
  params.window = handle_;
#endif
  manager_->Send(new GpuHostMsg_AcceleratedSurfacePostSubBuffer(params));
}

void ImageTransportHelper::SendAcceleratedSurfaceRelease(
    GpuHostMsg_AcceleratedSurfaceRelease_Params params) {
  params.surface_id = stub_->surface_id();
  params.route_id = route_id_;
  manager_->Send(new GpuHostMsg_AcceleratedSurfaceRelease(params));
}

void ImageTransportHelper::SendResizeView(const gfx::Size& size) {
  manager_->Send(new GpuHostMsg_ResizeView(stub_->surface_id(),
                                           route_id_,
                                           size));
}

void ImageTransportHelper::SetScheduled(bool is_scheduled) {
  gpu::GpuScheduler* scheduler = Scheduler();
  if (!scheduler)
    return;

  if (is_scheduled) {
    TRACE_EVENT_ASYNC_END0("gpu", "Descheduled", this);
  } else {
    TRACE_EVENT_ASYNC_BEGIN0("gpu", "Descheduled", this);
  }
  scheduler->SetScheduled(is_scheduled);
}

void ImageTransportHelper::DeferToFence(base::Closure task) {
  gpu::GpuScheduler* scheduler = Scheduler();
  DCHECK(scheduler);

  scheduler->DeferToFence(task);
}

bool ImageTransportHelper::MakeCurrent() {
  gpu::gles2::GLES2Decoder* decoder = Decoder();
  if (!decoder)
    return false;
  return decoder->MakeCurrent();
}

void ImageTransportHelper::SetSwapInterval() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableGpuVsync))
    Decoder()->GetGLContext()->SetSwapInterval(0);
  else
    Decoder()->GetGLContext()->SetSwapInterval(1);
}

void ImageTransportHelper::Suspend() {
  manager_->Send(new GpuHostMsg_AcceleratedSurfaceSuspend(stub_->surface_id()));
}

gpu::GpuScheduler* ImageTransportHelper::Scheduler() {
  if (!stub_.get())
    return NULL;
  return stub_->scheduler();
}

gpu::gles2::GLES2Decoder* ImageTransportHelper::Decoder() {
  if (!stub_.get())
    return NULL;
  return stub_->decoder();
}

void ImageTransportHelper::OnNewSurfaceACK(
    uint64 surface_handle,
    TransportDIB::Handle shm_handle) {
  surface_->OnNewSurfaceACK(surface_handle, shm_handle);
}

void ImageTransportHelper::OnBuffersSwappedACK() {
  surface_->OnBuffersSwappedACK();
}

void ImageTransportHelper::OnPostSubBufferACK() {
  surface_->OnPostSubBufferACK();
}

void ImageTransportHelper::OnResizeViewACK() {
  surface_->OnResizeViewACK();
}

void ImageTransportHelper::Resize(gfx::Size size) {
  // On windows, the surface is recreated and, in case the newly allocated
  // surface happens to have the same address, it should be invalidated on the
  // decoder so that future calls to MakeCurrent do not early out on the
  // assumption that neither the context or surface have actually changed.
#if defined(OS_WIN)
  Decoder()->ReleaseCurrent();
#endif

  surface_->OnResize(size);

#if defined(OS_ANDROID)
  manager_->gpu_memory_manager()->ScheduleManage();
#endif

#if defined(OS_WIN)
  Decoder()->MakeCurrent();
  SetSwapInterval();
#endif
}

PassThroughImageTransportSurface::PassThroughImageTransportSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    gfx::GLSurface* surface,
    bool transport)
    : GLSurfaceAdapter(surface),
      transport_(transport),
      did_set_swap_interval_(false) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         stub,
                                         gfx::kNullPluginWindow));
}

bool PassThroughImageTransportSurface::Initialize() {
  // The surface is assumed to have already been initialized.
  return helper_->Initialize();
}

void PassThroughImageTransportSurface::Destroy() {
  helper_->Destroy();
  GLSurfaceAdapter::Destroy();
}

bool PassThroughImageTransportSurface::SwapBuffers() {
  bool result = gfx::GLSurfaceAdapter::SwapBuffers();

  if (transport_) {
    // Round trip to the browser UI thread, for throttling, by sending a dummy
    // SwapBuffers message.
    GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
    params.surface_handle = 0;
#if defined(OS_WIN)
    params.size = GetSize();
#endif
    helper_->SendAcceleratedSurfaceBuffersSwapped(params);

    helper_->SetScheduled(false);
  }
  return result;
}

bool PassThroughImageTransportSurface::PostSubBuffer(
    int x, int y, int width, int height) {
  bool result = gfx::GLSurfaceAdapter::PostSubBuffer(x, y, width, height);

  if (transport_) {
    // Round trip to the browser UI thread, for throttling, by sending a dummy
    // PostSubBuffer message.
    GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params params;
    params.surface_handle = 0;
    params.x = x;
    params.y = y;
    params.width = width;
    params.height = height;
    helper_->SendAcceleratedSurfacePostSubBuffer(params);

    helper_->SetScheduled(false);
  }
  return result;
}

bool PassThroughImageTransportSurface::OnMakeCurrent(gfx::GLContext* context) {
  if (!did_set_swap_interval_) {
    helper_->SetSwapInterval();
    did_set_swap_interval_ = true;
  }
  return true;
}

void PassThroughImageTransportSurface::OnNewSurfaceACK(
    uint64 surface_handle,
    TransportDIB::Handle shm_handle) {
}

void PassThroughImageTransportSurface::OnBuffersSwappedACK() {
  DCHECK(transport_);
  helper_->SetScheduled(true);
}

void PassThroughImageTransportSurface::OnPostSubBufferACK() {
  DCHECK(transport_);
  helper_->SetScheduled(true);
}

void PassThroughImageTransportSurface::OnResizeViewACK() {
  DCHECK(transport_);
  Resize(new_size_);

  helper_->SetScheduled(true);
}

void PassThroughImageTransportSurface::OnResize(gfx::Size size) {
  new_size_ = size;

  if (transport_) {
    helper_->SendResizeView(size);
    helper_->SetScheduled(false);
  } else {
    Resize(new_size_);
  }
}

PassThroughImageTransportSurface::~PassThroughImageTransportSurface() {}

#endif  // defined(ENABLE_GPU)
