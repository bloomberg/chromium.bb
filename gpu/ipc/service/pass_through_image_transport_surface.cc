// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/pass_through_image_transport_surface.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_switches.h"

namespace gpu {

namespace {
// Number of swap generations before vsync is reenabled after we've stopped
// doing multiple swaps per frame.
const int kMultiWindowSwapEnableVSyncDelay = 60;

int g_current_swap_generation_ = 0;
int g_num_swaps_in_current_swap_generation_ = 0;
int g_last_multi_window_swap_generation_ = 0;
}  // anonymous namespace

PassThroughImageTransportSurface::PassThroughImageTransportSurface(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    gl::GLSurface* surface,
    MultiWindowSwapInterval multi_window_swap_interval)
    : GLSurfaceAdapter(surface),
      delegate_(delegate),
      multi_window_swap_interval_(multi_window_swap_interval),
      weak_ptr_factory_(this) {}

bool PassThroughImageTransportSurface::Initialize(
    gl::GLSurfaceFormat format) {
  // The surface is assumed to have already been initialized.
  delegate_->SetLatencyInfoCallback(
      base::Bind(&PassThroughImageTransportSurface::AddLatencyInfo,
                 base::Unretained(this)));
  return true;
}

void PassThroughImageTransportSurface::Destroy() {
  GLSurfaceAdapter::Destroy();
}

gfx::SwapResult PassThroughImageTransportSurface::SwapBuffers() {
  std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info =
      StartSwapBuffers();
  gfx::SwapResult result = gl::GLSurfaceAdapter::SwapBuffers();
  FinishSwapBuffers(std::move(latency_info), result);
  return result;
}

void PassThroughImageTransportSurface::SwapBuffersAsync(
    const GLSurface::SwapCompletionCallback& callback) {
  std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info =
      StartSwapBuffers();

  // We use WeakPtr here to avoid manual management of life time of an instance
  // of this class. Callback will not be called once the instance of this class
  // is destroyed. However, this also means that the callback can be run on
  // the calling thread only.
  gl::GLSurfaceAdapter::SwapBuffersAsync(base::Bind(
      &PassThroughImageTransportSurface::FinishSwapBuffersAsync,
      weak_ptr_factory_.GetWeakPtr(), base::Passed(&latency_info), callback));
}

gfx::SwapResult PassThroughImageTransportSurface::SwapBuffersWithBounds(
    const std::vector<gfx::Rect>& rects) {
  std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info =
      StartSwapBuffers();
  gfx::SwapResult result = gl::GLSurfaceAdapter::SwapBuffersWithBounds(rects);
  FinishSwapBuffers(std::move(latency_info), result);
  return result;
}

gfx::SwapResult PassThroughImageTransportSurface::PostSubBuffer(int x,
                                                                int y,
                                                                int width,
                                                                int height) {
  std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info =
      StartSwapBuffers();
  gfx::SwapResult result =
      gl::GLSurfaceAdapter::PostSubBuffer(x, y, width, height);
  FinishSwapBuffers(std::move(latency_info), result);
  return result;
}

void PassThroughImageTransportSurface::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    const GLSurface::SwapCompletionCallback& callback) {
  std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info =
      StartSwapBuffers();
  gl::GLSurfaceAdapter::PostSubBufferAsync(
      x, y, width, height,
      base::Bind(&PassThroughImageTransportSurface::FinishSwapBuffersAsync,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&latency_info),
                 callback));
}

gfx::SwapResult PassThroughImageTransportSurface::CommitOverlayPlanes() {
  std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info =
      StartSwapBuffers();
  gfx::SwapResult result = gl::GLSurfaceAdapter::CommitOverlayPlanes();
  FinishSwapBuffers(std::move(latency_info), result);
  return result;
}

void PassThroughImageTransportSurface::CommitOverlayPlanesAsync(
    const GLSurface::SwapCompletionCallback& callback) {
  std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info =
      StartSwapBuffers();
  gl::GLSurfaceAdapter::CommitOverlayPlanesAsync(base::Bind(
      &PassThroughImageTransportSurface::FinishSwapBuffersAsync,
      weak_ptr_factory_.GetWeakPtr(), base::Passed(&latency_info), callback));
}

PassThroughImageTransportSurface::~PassThroughImageTransportSurface() {
  if (delegate_) {
    delegate_->SetLatencyInfoCallback(
        base::Callback<void(const std::vector<ui::LatencyInfo>&)>());
  }
}

void PassThroughImageTransportSurface::AddLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  latency_info_.insert(latency_info_.end(), latency_info.begin(),
                       latency_info.end());
}

void PassThroughImageTransportSurface::SendVSyncUpdateIfAvailable() {
  gfx::VSyncProvider* vsync_provider = GetVSyncProvider();
  if (vsync_provider) {
    vsync_provider->GetVSyncParameters(base::Bind(
        &ImageTransportSurfaceDelegate::UpdateVSyncParameters, delegate_));
  }
}

void PassThroughImageTransportSurface::UpdateSwapInterval() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuVsync)) {
    gl::GLContext::GetCurrent()->ForceSwapIntervalZero(true);
    return;
  }

  gl::GLContext::GetCurrent()->SetSwapInterval(1);

  if (multi_window_swap_interval_ == kMultiWindowSwapIntervalForceZero) {
    // This code is a simple way of enforcing that we only vsync if one surface
    // is swapping per frame. This provides single window cases a stable refresh
    // while allowing multi-window cases to not slow down due to multiple syncs
    // on a single thread. A better way to fix this problem would be to have
    // each surface present on its own thread.

    if (g_current_swap_generation_ == swap_generation_) {
      // No other surface has swapped since we swapped last time.
      if (g_num_swaps_in_current_swap_generation_ > 1)
        g_last_multi_window_swap_generation_ = g_current_swap_generation_;
      g_num_swaps_in_current_swap_generation_ = 0;
      g_current_swap_generation_++;
    }

    swap_generation_ = g_current_swap_generation_;
    g_num_swaps_in_current_swap_generation_++;

    bool should_override_vsync =
        (g_num_swaps_in_current_swap_generation_ > 1) ||
        (g_current_swap_generation_ - g_last_multi_window_swap_generation_ <
         kMultiWindowSwapEnableVSyncDelay);
    gl::GLContext::GetCurrent()->ForceSwapIntervalZero(should_override_vsync);
  }
}

std::unique_ptr<std::vector<ui::LatencyInfo>>
PassThroughImageTransportSurface::StartSwapBuffers() {
  // GetVsyncValues before SwapBuffers to work around Mali driver bug:
  // crbug.com/223558.
  SendVSyncUpdateIfAvailable();

  UpdateSwapInterval();

  base::TimeTicks swap_time = base::TimeTicks::Now();
  for (auto& latency : latency_info_) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0, 0, swap_time, 1);
  }

  std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info(
      new std::vector<ui::LatencyInfo>());
  latency_info->swap(latency_info_);

  return latency_info;
}

void PassThroughImageTransportSurface::FinishSwapBuffers(
    std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info,
    gfx::SwapResult result) {
  base::TimeTicks swap_ack_time = base::TimeTicks::Now();
  bool has_browser_snapshot_request = false;
  for (auto& latency : *latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0,
        swap_ack_time, 1);
    has_browser_snapshot_request |= latency.FindLatency(
        ui::BROWSER_SNAPSHOT_FRAME_NUMBER_COMPONENT, nullptr);
  }
  if (has_browser_snapshot_request)
    WaitForSnapshotRendering();

  if (delegate_) {
    SwapBuffersCompleteParams params;
    params.latency_info = std::move(*latency_info);
    params.result = result;
    delegate_->DidSwapBuffersComplete(std::move(params));
  }
}

void PassThroughImageTransportSurface::FinishSwapBuffersAsync(
    std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info,
    GLSurface::SwapCompletionCallback callback,
    gfx::SwapResult result) {
  FinishSwapBuffers(std::move(latency_info), result);
  callback.Run(result);
}

}  // namespace gpu
