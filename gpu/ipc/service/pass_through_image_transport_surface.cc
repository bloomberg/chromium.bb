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

bool HasSwitch(const char switch_constant[]) {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(switch_constant);
}

}  // anonymous namespace

PassThroughImageTransportSurface::PassThroughImageTransportSurface(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    gl::GLSurface* surface,
    MultiWindowSwapInterval multi_window_swap_interval)
    : GLSurfaceAdapter(surface),
      is_gpu_vsync_disabled_(HasSwitch(switches::kDisableGpuVsync)),
      is_presentation_callback_enabled_(
          HasSwitch(switches::kEnablePresentationCallback)),
      delegate_(delegate),
      multi_window_swap_interval_(multi_window_swap_interval),
      weak_ptr_factory_(this) {}

PassThroughImageTransportSurface::~PassThroughImageTransportSurface() {
  if (delegate_)
    delegate_->SetSnapshotRequestedCallback(base::Closure());
}

bool PassThroughImageTransportSurface::Initialize(
    gl::GLSurfaceFormat format) {
  DCHECK(!is_presentation_callback_enabled_ ||
         gl::GLSurfaceAdapter::SupportsPresentationCallback());
  // The surface is assumed to have already been initialized.
  delegate_->SetSnapshotRequestedCallback(
      base::Bind(&PassThroughImageTransportSurface::SetSnapshotRequested,
                 base::Unretained(this)));
  return true;
}

void PassThroughImageTransportSurface::Destroy() {
  GLSurfaceAdapter::Destroy();
}

gfx::SwapResult PassThroughImageTransportSurface::SwapBuffers(
    const PresentationCallback& callback) {
  gfx::SwapResponse response;
  StartSwapBuffers(&response);
  gfx::SwapResult result = gl::GLSurfaceAdapter::SwapBuffers(
      base::Bind(&PassThroughImageTransportSurface::BufferPresented,
                 weak_ptr_factory_.GetWeakPtr(), response.swap_id, callback));
  response.result = result;
  FinishSwapBuffers(GetAndResetSnapshotRequested(), std::move(response));
  return result;
}

void PassThroughImageTransportSurface::SwapBuffersAsync(
    const SwapCompletionCallback& completion_callback,
    const PresentationCallback& presentation_callback) {
  gfx::SwapResponse response;
  StartSwapBuffers(&response);

  // We use WeakPtr here to avoid manual management of life time of an instance
  // of this class. Callback will not be called once the instance of this class
  // is destroyed. However, this also means that the callback can be run on
  // the calling thread only.
  uint64_t swap_id = response.swap_id;
  gl::GLSurfaceAdapter::SwapBuffersAsync(
      base::Bind(&PassThroughImageTransportSurface::FinishSwapBuffersAsync,
                 weak_ptr_factory_.GetWeakPtr(), completion_callback,
                 GetAndResetSnapshotRequested(), base::Passed(&response)),
      base::Bind(&PassThroughImageTransportSurface::BufferPresented,
                 weak_ptr_factory_.GetWeakPtr(), swap_id,
                 presentation_callback));
}

gfx::SwapResult PassThroughImageTransportSurface::SwapBuffersWithBounds(
    const std::vector<gfx::Rect>& rects,
    const PresentationCallback& callback) {
  gfx::SwapResponse response;
  StartSwapBuffers(&response);
  const uint64_t swap_id = response.swap_id;
  gfx::SwapResult result = gl::GLSurfaceAdapter::SwapBuffersWithBounds(
      rects, base::Bind(&PassThroughImageTransportSurface::BufferPresented,
                        weak_ptr_factory_.GetWeakPtr(), swap_id, callback));
  response.result = result;
  FinishSwapBuffers(GetAndResetSnapshotRequested(), std::move(response));
  return result;
}

gfx::SwapResult PassThroughImageTransportSurface::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    const PresentationCallback& callback) {
  gfx::SwapResponse response;
  StartSwapBuffers(&response);
  const uint64_t swap_id = response.swap_id;
  gfx::SwapResult result = gl::GLSurfaceAdapter::PostSubBuffer(
      x, y, width, height,
      base::Bind(&PassThroughImageTransportSurface::BufferPresented,
                 weak_ptr_factory_.GetWeakPtr(), swap_id, callback));
  response.result = result;
  FinishSwapBuffers(GetAndResetSnapshotRequested(), std::move(response));

  return result;
}

void PassThroughImageTransportSurface::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    const GLSurface::SwapCompletionCallback& completion_callback,
    const PresentationCallback& presentation_callback) {
  gfx::SwapResponse response;
  StartSwapBuffers(&response);
  const uint64_t swap_id = response.swap_id;
  gl::GLSurfaceAdapter::PostSubBufferAsync(
      x, y, width, height,
      base::Bind(&PassThroughImageTransportSurface::FinishSwapBuffersAsync,
                 weak_ptr_factory_.GetWeakPtr(), completion_callback,
                 GetAndResetSnapshotRequested(), base::Passed(&response)),
      base::Bind(&PassThroughImageTransportSurface::BufferPresented,
                 weak_ptr_factory_.GetWeakPtr(), swap_id,
                 presentation_callback));
}

gfx::SwapResult PassThroughImageTransportSurface::CommitOverlayPlanes(
    const PresentationCallback& callback) {
  gfx::SwapResponse response;
  StartSwapBuffers(&response);
  const uint64_t swap_id = response.swap_id;
  gfx::SwapResult result = gl::GLSurfaceAdapter::CommitOverlayPlanes(
      base::Bind(&PassThroughImageTransportSurface::BufferPresented,
                 weak_ptr_factory_.GetWeakPtr(), swap_id, callback));
  response.result = result;
  FinishSwapBuffers(GetAndResetSnapshotRequested(), std::move(response));
  return result;
}

void PassThroughImageTransportSurface::CommitOverlayPlanesAsync(
    const GLSurface::SwapCompletionCallback& callback,
    const PresentationCallback& presentation_callback) {
  gfx::SwapResponse response;
  StartSwapBuffers(&response);
  const uint64_t swap_id = response.swap_id;
  gl::GLSurfaceAdapter::CommitOverlayPlanesAsync(
      base::Bind(&PassThroughImageTransportSurface::FinishSwapBuffersAsync,
                 weak_ptr_factory_.GetWeakPtr(), callback,
                 GetAndResetSnapshotRequested(), base::Passed(&response)),
      base::Bind(&PassThroughImageTransportSurface::BufferPresented,
                 weak_ptr_factory_.GetWeakPtr(), swap_id,
                 presentation_callback));
}

void PassThroughImageTransportSurface::SetSnapshotRequested() {
  snapshot_requested_ = true;
}

bool PassThroughImageTransportSurface::GetAndResetSnapshotRequested() {
  bool sr = snapshot_requested_;
  snapshot_requested_ = false;
  return sr;
}

void PassThroughImageTransportSurface::SendVSyncUpdateIfAvailable() {
  // When PresentationCallback is enabled, we will get VSYNC parameters from the
  // PresentationCallback, so the VSYNC update is not necessary anymore.
  if (is_presentation_callback_enabled_)
    return;

  gfx::VSyncProvider* vsync_provider = GetVSyncProvider();
  if (vsync_provider) {
    vsync_provider->GetVSyncParameters(base::Bind(
        &ImageTransportSurfaceDelegate::UpdateVSyncParameters, delegate_));
  }
}

void PassThroughImageTransportSurface::UpdateSwapInterval() {
  if (is_gpu_vsync_disabled_) {
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

void PassThroughImageTransportSurface::StartSwapBuffers(
    gfx::SwapResponse* response) {
  // GetVsyncValues before SwapBuffers to work around Mali driver bug:
  // crbug.com/223558.
  SendVSyncUpdateIfAvailable();
  UpdateSwapInterval();

  response->swap_id = swap_id_++;
  response->swap_start = base::TimeTicks::Now();
}

void PassThroughImageTransportSurface::FinishSwapBuffers(
    bool snapshot_requested,
    gfx::SwapResponse response) {
  response.swap_end = base::TimeTicks::Now();
  if (snapshot_requested)
    WaitForSnapshotRendering();

  if (delegate_) {
    SwapBuffersCompleteParams params;
    params.response = std::move(response);
    delegate_->DidSwapBuffersComplete(std::move(params));
  }
}

void PassThroughImageTransportSurface::FinishSwapBuffersAsync(
    GLSurface::SwapCompletionCallback callback,
    bool snapshot_requested,
    gfx::SwapResponse response,
    gfx::SwapResult result) {
  response.result = result;
  FinishSwapBuffers(snapshot_requested, std::move(response));
  callback.Run(result);
}

void PassThroughImageTransportSurface::BufferPresented(
    uint64_t swap_id,
    const GLSurface::PresentationCallback& callback,
    const gfx::PresentationFeedback& feedback) {
  if (!is_presentation_callback_enabled_)
    return;
  callback.Run(feedback);
  if (delegate_)
    delegate_->BufferPresented(swap_id, feedback);
}

}  // namespace gpu
