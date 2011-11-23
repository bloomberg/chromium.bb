// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "content/common/gpu/image_transport_surface.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_messages.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"

ImageTransportSurface::ImageTransportSurface() {
}

ImageTransportSurface::~ImageTransportSurface() {
}

ImageTransportHelper::ImageTransportHelper(ImageTransportSurface* surface,
                                           GpuChannelManager* manager,
                                           int32 render_view_id,
                                           int32 renderer_id,
                                           int32 command_buffer_id,
                                           gfx::PluginWindowHandle handle)
    : surface_(surface),
      manager_(manager),
      render_view_id_(render_view_id),
      renderer_id_(renderer_id),
      command_buffer_id_(command_buffer_id),
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

void ImageTransportHelper::Destroy() {
}

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

void ImageTransportHelper::SendAcceleratedSurfaceRelease(
    GpuHostMsg_AcceleratedSurfaceRelease_Params params) {
  params.renderer_id = renderer_id_;
  params.render_view_id = render_view_id_;
  params.route_id = route_id_;
  manager_->Send(new GpuHostMsg_AcceleratedSurfaceRelease(params));
}

void ImageTransportHelper::SendAcceleratedSurfaceNew(
    GpuHostMsg_AcceleratedSurfaceNew_Params params) {
  params.renderer_id = renderer_id_;
  params.render_view_id = render_view_id_;
  params.route_id = route_id_;
#if defined(OS_MACOSX)
  params.window = handle_;
#endif
  manager_->Send(new GpuHostMsg_AcceleratedSurfaceNew(params));
}

void ImageTransportHelper::SendAcceleratedSurfaceBuffersSwapped(
    GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params) {
  params.renderer_id = renderer_id_;
  params.render_view_id = render_view_id_;
  params.route_id = route_id_;
#if defined(OS_MACOSX)
  params.window = handle_;
#endif
  manager_->Send(new GpuHostMsg_AcceleratedSurfaceBuffersSwapped(params));
}

void ImageTransportHelper::SendAcceleratedSurfacePostSubBuffer(
    GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params params) {
  params.renderer_id = renderer_id_;
  params.render_view_id = render_view_id_;
  params.route_id = route_id_;
#if defined(OS_MACOSX)
  params.window = handle_;
#endif
  manager_->Send(new GpuHostMsg_AcceleratedSurfacePostSubBuffer(params));
}

void ImageTransportHelper::SendResizeView(const gfx::Size& size) {
  manager_->Send(new GpuHostMsg_ResizeView(renderer_id_,
                                           render_view_id_,
                                           route_id_,
                                           size));
}

void ImageTransportHelper::SetScheduled(bool is_scheduled) {
  gpu::GpuScheduler* scheduler = Scheduler();
  if (!scheduler)
    return;

  scheduler->SetScheduled(is_scheduled);
}

void ImageTransportHelper::DeferToFence(base::Closure task) {
  gpu::GpuScheduler* scheduler = Scheduler();
  DCHECK(scheduler);

  scheduler->DeferToFence(task);
}

void ImageTransportHelper::OnBuffersSwappedACK() {
  surface_->OnBuffersSwappedACK();
}

void ImageTransportHelper::OnPostSubBufferACK() {
  surface_->OnPostSubBufferACK();
}

void ImageTransportHelper::OnNewSurfaceACK(
    uint64 surface_id,
    TransportDIB::Handle shm_handle) {
  surface_->OnNewSurfaceACK(surface_id, shm_handle);
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

#if defined(OS_WIN)
  Decoder()->MakeCurrent();
  SetSwapInterval();
#endif
}

void ImageTransportHelper::SetSwapInterval() {
  GpuChannel* channel = manager_->LookupChannel(renderer_id_);
  if (!channel)
    return;

  GpuCommandBufferStub* stub =
      channel->LookupCommandBuffer(command_buffer_id_);
  if (!stub)
    return;

  stub->SetSwapInterval();
}

bool ImageTransportHelper::MakeCurrent() {
  gpu::gles2::GLES2Decoder* decoder = Decoder();
  if (!decoder)
    return false;
  return decoder->MakeCurrent();
}

gpu::GpuScheduler* ImageTransportHelper::Scheduler() {
  GpuChannel* channel = manager_->LookupChannel(renderer_id_);
  if (!channel)
    return NULL;

  GpuCommandBufferStub* stub =
      channel->LookupCommandBuffer(command_buffer_id_);
  if (!stub)
    return NULL;

  return stub->scheduler();
}

gpu::gles2::GLES2Decoder* ImageTransportHelper::Decoder() {
  GpuChannel* channel = manager_->LookupChannel(renderer_id_);
  if (!channel)
    return NULL;

  GpuCommandBufferStub* stub =
      channel->LookupCommandBuffer(command_buffer_id_);
  if (!stub)
    return NULL;

  return stub->decoder();
}

PassThroughImageTransportSurface::PassThroughImageTransportSurface(
    GpuChannelManager* manager,
    int32 render_view_id,
    int32 renderer_id,
    int32 command_buffer_id,
    gfx::GLSurface* surface) : GLSurfaceAdapter(surface) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         render_view_id,
                                         renderer_id,
                                         command_buffer_id,
                                         gfx::kNullPluginWindow));
}

PassThroughImageTransportSurface::~PassThroughImageTransportSurface() {
}

bool PassThroughImageTransportSurface::Initialize() {
  // The surface is assumed to have already been initialized.
  return helper_->Initialize();
}

void PassThroughImageTransportSurface::Destroy() {
  helper_->Destroy();
  GLSurfaceAdapter::Destroy();
}

void PassThroughImageTransportSurface::OnNewSurfaceACK(
    uint64 surface_id, TransportDIB::Handle surface_handle) {
}

void PassThroughImageTransportSurface::OnBuffersSwappedACK() {
  NOTREACHED();
}

void PassThroughImageTransportSurface::OnPostSubBufferACK() {
  NOTREACHED();
}

void PassThroughImageTransportSurface::OnResizeViewACK() {
  helper_->SetScheduled(true);
}

void PassThroughImageTransportSurface::OnResize(gfx::Size size) {
  helper_->SendResizeView(size);
  helper_->SetScheduled(false);
}

#endif  // defined(ENABLE_GPU)
