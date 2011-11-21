// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "content/common/gpu/image_transport_surface.h"

#include "base/callback.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_messages.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"

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
       NewCallback(this, &ImageTransportHelper::Resize));

  return true;
}

void ImageTransportHelper::Destroy() {
}

bool ImageTransportHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ImageTransportHelper, message)
    IPC_MESSAGE_HANDLER(AcceleratedSurfaceMsg_BuffersSwappedACK,
                        OnBuffersSwappedACK)
    IPC_MESSAGE_HANDLER(AcceleratedSurfaceMsg_NewACK,
                        OnNewSurfaceACK)
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

void ImageTransportHelper::SetScheduled(bool is_scheduled) {
  gpu::GpuScheduler* scheduler = Scheduler();
  if (!scheduler)
    return;

  scheduler->SetScheduled(is_scheduled);
}

void ImageTransportHelper::OnBuffersSwappedACK() {
  surface_->OnBuffersSwappedACK();
}

void ImageTransportHelper::OnNewSurfaceACK(
    uint64 surface_id,
    TransportDIB::Handle shm_handle) {
  surface_->OnNewSurfaceACK(surface_id, shm_handle);
}

void ImageTransportHelper::Resize(gfx::Size size) {
  surface_->OnResize(size);
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

#endif  // defined(ENABLE_GPU)
