// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_PASS_THROUGH_IMAGE_TRANSPORT_SURFACE_H_
#define GPU_IPC_SERVICE_PASS_THROUGH_IMAGE_TRANSPORT_SURFACE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "ui/gl/gl_surface.h"

namespace gpu {

enum MultiWindowSwapInterval {
  // Use the default swap interval of 1 even if multiple windows are swapping.
  // This can reduce frame rate if the swap buffers calls block.
  kMultiWindowSwapIntervalDefault,
  // Force swap interval to 0 when multiple windows are swapping.
  kMultiWindowSwapIntervalForceZero
};

// An implementation of ImageTransportSurface that implements GLSurface through
// GLSurfaceAdapter, thereby forwarding GLSurface methods through to it.
class PassThroughImageTransportSurface : public gl::GLSurfaceAdapter {
 public:
  PassThroughImageTransportSurface(
      base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
      gl::GLSurface* surface,
      MultiWindowSwapInterval multi_window_swap_interval);

  // GLSurface implementation.
  bool Initialize(gl::GLSurfaceFormat format) override;
  void Destroy() override;
  gfx::SwapResult SwapBuffers() override;
  void SwapBuffersAsync(const SwapCompletionCallback& callback) override;
  gfx::SwapResult SwapBuffersWithBounds(
      const std::vector<gfx::Rect>& rects) override;
  gfx::SwapResult PostSubBuffer(int x, int y, int width, int height) override;
  void PostSubBufferAsync(int x,
                          int y,
                          int width,
                          int height,
                          const SwapCompletionCallback& callback) override;
  gfx::SwapResult CommitOverlayPlanes() override;
  void CommitOverlayPlanesAsync(
      const SwapCompletionCallback& callback) override;

 private:
  ~PassThroughImageTransportSurface() override;

  void SetSnapshotRequested();
  bool GetAndResetSnapshotRequested();

  // If updated vsync parameters can be determined, send this information to
  // the browser.
  void SendVSyncUpdateIfAvailable();

  void UpdateSwapInterval();

  void StartSwapBuffers(gfx::SwapResponse* response);
  void FinishSwapBuffers(bool snapshot_requested, gfx::SwapResponse response);
  void FinishSwapBuffersAsync(GLSurface::SwapCompletionCallback callback,
                              bool snapshot_requested,
                              gfx::SwapResponse response,
                              gfx::SwapResult result);

  base::WeakPtr<ImageTransportSurfaceDelegate> delegate_;
  uint64_t swap_id_ = 0;
  bool snapshot_requested_ = false;
  MultiWindowSwapInterval multi_window_swap_interval_ =
      kMultiWindowSwapIntervalDefault;
  int swap_generation_ = 0;

  base::WeakPtrFactory<PassThroughImageTransportSurface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PassThroughImageTransportSurface);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_PASS_THROUGH_IMAGE_TRANSPORT_SURFACE_H_
