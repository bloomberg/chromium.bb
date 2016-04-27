// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_
#define GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_

#include <list>
#include <memory>
#include <vector>

#import "base/mac/scoped_nsobject.h"
#include "base/timer/timer.h"
#include "gpu/ipc/service/gpu_command_buffer_stub.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/events/latency_info.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_switching_observer.h"

@class CAContext;
@class CALayer;

namespace ui {
class CALayerTreeCoordinator;
}

namespace gpu {

class ImageTransportSurfaceOverlayMac : public gfx::GLSurface,
                                        public ui::GpuSwitchingObserver {
 public:
  ImageTransportSurfaceOverlayMac(GpuChannelManager* manager,
                                  GpuCommandBufferStub* stub,
                                  SurfaceHandle handle);

  // GLSurface implementation
  bool Initialize(gfx::GLSurface::Format format) override;
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              bool has_alpha) override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers() override;
  gfx::SwapResult PostSubBuffer(int x, int y, int width, int height) override;
  bool SupportsPostSubBuffer() override;
  gfx::Size GetSize() override;
  void* GetHandle() override;
  bool OnMakeCurrent(gfx::GLContext* context) override;
  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            gl::GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override;
  bool ScheduleCALayer(gl::GLImage* contents_image,
                       const gfx::RectF& contents_rect,
                       float opacity,
                       unsigned background_color,
                       unsigned edge_aa_mask,
                       const gfx::RectF& rect,
                       bool is_clipped,
                       const gfx::RectF& clip_rect,
                       const gfx::Transform& transform,
                       int sorting_context_id,
                       unsigned filter) override;
  bool IsSurfaceless() const override;

  // ui::GpuSwitchingObserver implementation.
  void OnGpuSwitched() override;

 private:
  ~ImageTransportSurfaceOverlayMac() override;

  void SetLatencyInfo(const std::vector<ui::LatencyInfo>& latency_info);
  void BufferPresented(gpu::SurfaceHandle surface_handle,
                       const base::TimeTicks& vsync_timebase,
                       const base::TimeDelta& vsync_interval);
  void SendAcceleratedSurfaceBuffersSwapped(
      gpu::SurfaceHandle surface_handle,
      CAContextID ca_context_id,
      const gfx::ScopedRefCountedIOSurfaceMachPort& io_surface,
      const gfx::Size& size,
      float scale_factor,
      std::vector<ui::LatencyInfo> latency_info);
  gfx::SwapResult SwapBuffersInternal(const gfx::Rect& pixel_damage_rect);

  GpuChannelManager* manager_;
  base::WeakPtr<GpuCommandBufferStub> stub_;
  SurfaceHandle handle_;
  std::vector<ui::LatencyInfo> latency_info_;

  bool use_remote_layer_api_;
  base::scoped_nsobject<CAContext> ca_context_;
  std::unique_ptr<ui::CALayerTreeCoordinator> ca_layer_tree_coordinator_;

  gfx::Size pixel_size_;
  float scale_factor_;

  // The renderer ID that all contexts made current to this surface should be
  // targeting.
  GLint gl_renderer_id_;

  // The vsync information provided by the browser.
  bool vsync_parameters_valid_;
  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_
