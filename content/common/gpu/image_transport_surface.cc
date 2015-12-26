// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_messages.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"

namespace content {

ImageTransportSurface::ImageTransportSurface() {}

ImageTransportSurface::~ImageTransportSurface() {}

scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle) {
  scoped_refptr<gfx::GLSurface> surface;
  if (handle.transport_type == gfx::NULL_TRANSPORT) {
    surface = manager->GetDefaultOffscreenSurface();
  } else {
    surface = CreateNativeSurface(manager, stub, handle);
    if (!surface.get() || !surface->Initialize())
      return NULL;
  }

  return surface;
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
  if (stub_.get()) {
    stub_->SetLatencyInfoCallback(
        base::Callback<void(const std::vector<ui::LatencyInfo>&)>());
  }
  manager_->RemoveRoute(route_id_);
}

bool ImageTransportHelper::Initialize() {
  gpu::gles2::GLES2Decoder* decoder = Decoder();

  if (!decoder)
    return false;

  stub_->SetLatencyInfoCallback(
      base::Bind(&ImageTransportHelper::SetLatencyInfo,
                 base::Unretained(this)));

  return true;
}

bool ImageTransportHelper::OnMessageReceived(const IPC::Message& message) {
#if defined(OS_MACOSX)
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ImageTransportHelper, message)
    IPC_MESSAGE_HANDLER(AcceleratedSurfaceMsg_BufferPresented,
                        OnBufferPresented)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
#else
  NOTREACHED();
  return false;
#endif
}

#if defined(OS_MACOSX)
void ImageTransportHelper::SendAcceleratedSurfaceBuffersSwapped(
    GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params) {
  // TRACE_EVENT for gpu tests:
  TRACE_EVENT_INSTANT2("test_gpu", "SwapBuffers",
                       TRACE_EVENT_SCOPE_THREAD,
                       "GLImpl", static_cast<int>(gfx::GetGLImplementation()),
                       "width", params.size.width());
  // On mac, handle_ is a surface id. See
  // GpuProcessTransportFactory::CreatePerCompositorData
  params.surface_id = handle_;
  params.route_id = route_id_;
  manager_->Send(new GpuHostMsg_AcceleratedSurfaceBuffersSwapped(params));
}
#endif

bool ImageTransportHelper::MakeCurrent() {
  gpu::gles2::GLES2Decoder* decoder = Decoder();
  if (!decoder)
    return false;
  return decoder->MakeCurrent();
}

void ImageTransportHelper::SetSwapInterval(gfx::GLContext* context) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuVsync))
    context->ForceSwapIntervalZero(true);
  else
    context->SetSwapInterval(1);
}

gpu::gles2::GLES2Decoder* ImageTransportHelper::Decoder() {
  if (!stub_.get())
    return NULL;
  return stub_->decoder();
}

#if defined(OS_MACOSX)
void ImageTransportHelper::OnBufferPresented(
    const AcceleratedSurfaceMsg_BufferPresented_Params& params) {
  surface_->OnBufferPresented(params);
}
#endif

void ImageTransportHelper::SetLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  surface_->SetLatencyInfo(latency_info);
}

PassThroughImageTransportSurface::PassThroughImageTransportSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    gfx::GLSurface* surface)
    : GLSurfaceAdapter(surface),
      did_set_swap_interval_(false),
      weak_ptr_factory_(this) {
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
  GLSurfaceAdapter::Destroy();
}

void PassThroughImageTransportSurface::SetLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  for (size_t i = 0; i < latency_info.size(); i++)
    latency_info_.push_back(latency_info[i]);
}

gfx::SwapResult PassThroughImageTransportSurface::SwapBuffers() {
  scoped_ptr<std::vector<ui::LatencyInfo>> latency_info = StartSwapBuffers();
  gfx::SwapResult result = gfx::GLSurfaceAdapter::SwapBuffers();
  FinishSwapBuffers(std::move(latency_info), result);
  return result;
}

void PassThroughImageTransportSurface::SwapBuffersAsync(
    const GLSurface::SwapCompletionCallback& callback) {
  scoped_ptr<std::vector<ui::LatencyInfo>> latency_info = StartSwapBuffers();

  // We use WeakPtr here to avoid manual management of life time of an instance
  // of this class. Callback will not be called once the instance of this class
  // is destroyed. However, this also means that the callback can be run on
  // the calling thread only.
  gfx::GLSurfaceAdapter::SwapBuffersAsync(base::Bind(
      &PassThroughImageTransportSurface::FinishSwapBuffersAsync,
      weak_ptr_factory_.GetWeakPtr(), base::Passed(&latency_info), callback));
}

gfx::SwapResult PassThroughImageTransportSurface::PostSubBuffer(int x,
                                                                int y,
                                                                int width,
                                                                int height) {
  scoped_ptr<std::vector<ui::LatencyInfo>> latency_info = StartSwapBuffers();
  gfx::SwapResult result =
      gfx::GLSurfaceAdapter::PostSubBuffer(x, y, width, height);
  FinishSwapBuffers(std::move(latency_info), result);
  return result;
}

void PassThroughImageTransportSurface::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    const GLSurface::SwapCompletionCallback& callback) {
  scoped_ptr<std::vector<ui::LatencyInfo>> latency_info = StartSwapBuffers();
  gfx::GLSurfaceAdapter::PostSubBufferAsync(
      x, y, width, height,
      base::Bind(&PassThroughImageTransportSurface::FinishSwapBuffersAsync,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&latency_info),
                 callback));
}

gfx::SwapResult PassThroughImageTransportSurface::CommitOverlayPlanes() {
  scoped_ptr<std::vector<ui::LatencyInfo>> latency_info = StartSwapBuffers();
  gfx::SwapResult result = gfx::GLSurfaceAdapter::CommitOverlayPlanes();
  FinishSwapBuffers(std::move(latency_info), result);
  return result;
}

void PassThroughImageTransportSurface::CommitOverlayPlanesAsync(
    const GLSurface::SwapCompletionCallback& callback) {
  scoped_ptr<std::vector<ui::LatencyInfo>> latency_info = StartSwapBuffers();
  gfx::GLSurfaceAdapter::CommitOverlayPlanesAsync(base::Bind(
      &PassThroughImageTransportSurface::FinishSwapBuffersAsync,
      weak_ptr_factory_.GetWeakPtr(), base::Passed(&latency_info), callback));
}

bool PassThroughImageTransportSurface::OnMakeCurrent(gfx::GLContext* context) {
  if (!did_set_swap_interval_) {
    ImageTransportHelper::SetSwapInterval(context);
    did_set_swap_interval_ = true;
  }
  return true;
}

#if defined(OS_MACOSX)
void PassThroughImageTransportSurface::OnBufferPresented(
    const AcceleratedSurfaceMsg_BufferPresented_Params& /* params */) {
  NOTREACHED();
}
#endif

gfx::Size PassThroughImageTransportSurface::GetSize() {
  return GLSurfaceAdapter::GetSize();
}

PassThroughImageTransportSurface::~PassThroughImageTransportSurface() {}

void PassThroughImageTransportSurface::SendVSyncUpdateIfAvailable() {
  gfx::VSyncProvider* vsync_provider = GetVSyncProvider();
  if (vsync_provider) {
    vsync_provider->GetVSyncParameters(
        base::Bind(&GpuCommandBufferStub::SendUpdateVSyncParameters,
                   helper_->stub()->AsWeakPtr()));
  }
}

scoped_ptr<std::vector<ui::LatencyInfo>>
PassThroughImageTransportSurface::StartSwapBuffers() {
  // GetVsyncValues before SwapBuffers to work around Mali driver bug:
  // crbug.com/223558.
  SendVSyncUpdateIfAvailable();

  base::TimeTicks swap_time = base::TimeTicks::Now();
  for (auto& latency : latency_info_) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0, 0, swap_time, 1);
  }

  scoped_ptr<std::vector<ui::LatencyInfo>> latency_info(
      new std::vector<ui::LatencyInfo>());
  latency_info->swap(latency_info_);

  return latency_info;
}

void PassThroughImageTransportSurface::FinishSwapBuffers(
    scoped_ptr<std::vector<ui::LatencyInfo>> latency_info,
    gfx::SwapResult result) {
  base::TimeTicks swap_ack_time = base::TimeTicks::Now();
  for (auto& latency : *latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0,
        swap_ack_time, 1);
  }

  helper_->stub()->SendSwapBuffersCompleted(*latency_info, result);
}

void PassThroughImageTransportSurface::FinishSwapBuffersAsync(
    scoped_ptr<std::vector<ui::LatencyInfo>> latency_info,
    GLSurface::SwapCompletionCallback callback,
    gfx::SwapResult result) {
  FinishSwapBuffers(std::move(latency_info), result);
  callback.Run(result);
}

}  // namespace content
