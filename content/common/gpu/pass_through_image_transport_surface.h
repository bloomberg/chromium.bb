// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_PASS_THROUGH_IMAGE_TRANSPORT_SURFACE_H_
#define CONTENT_COMMON_GPU_PASS_THROUGH_IMAGE_TRANSPORT_SURFACE_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/gpu/image_transport_surface.h"
#include "ui/events/latency_info.h"
#include "ui/gl/gl_surface.h"

namespace content {
class GpuChannelManager;
class GpuCommandBufferStub;

// An implementation of ImageTransportSurface that implements GLSurface through
// GLSurfaceAdapter, thereby forwarding GLSurface methods through to it.
class PassThroughImageTransportSurface : public gfx::GLSurfaceAdapter {
 public:
  PassThroughImageTransportSurface(GpuChannelManager* manager,
                                   GpuCommandBufferStub* stub,
                                   gfx::GLSurface* surface);

  // GLSurface implementation.
  bool Initialize(gfx::GLSurface::Format format) override;
  void Destroy() override;
  gfx::SwapResult SwapBuffers() override;
  void SwapBuffersAsync(const SwapCompletionCallback& callback) override;
  gfx::SwapResult PostSubBuffer(int x, int y, int width, int height) override;
  void PostSubBufferAsync(int x,
                          int y,
                          int width,
                          int height,
                          const SwapCompletionCallback& callback) override;
  gfx::SwapResult CommitOverlayPlanes() override;
  void CommitOverlayPlanesAsync(
      const SwapCompletionCallback& callback) override;
  bool OnMakeCurrent(gfx::GLContext* context) override;

 private:
  ~PassThroughImageTransportSurface() override;

  // If updated vsync parameters can be determined, send this information to
  // the browser.
  void SendVSyncUpdateIfAvailable();

  void SetLatencyInfo(const std::vector<ui::LatencyInfo>& latency_info);
  scoped_ptr<std::vector<ui::LatencyInfo>> StartSwapBuffers();
  void FinishSwapBuffers(scoped_ptr<std::vector<ui::LatencyInfo>> latency_info,
                         gfx::SwapResult result);
  void FinishSwapBuffersAsync(
      scoped_ptr<std::vector<ui::LatencyInfo>> latency_info,
      GLSurface::SwapCompletionCallback callback,
      gfx::SwapResult result);

  base::WeakPtr<GpuCommandBufferStub> stub_;
  bool did_set_swap_interval_;
  std::vector<ui::LatencyInfo> latency_info_;
  base::WeakPtrFactory<PassThroughImageTransportSurface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PassThroughImageTransportSurface);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_PASS_THROUGH_IMAGE_TRANSPORT_SURFACE_H_
