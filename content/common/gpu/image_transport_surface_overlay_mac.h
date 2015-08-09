// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_
#define CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_

#import "base/mac/scoped_nsobject.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/image_transport_surface.h"
#include "ui/gl/gl_surface.h"

@class CAContext;
@class CALayer;

namespace content {

class ImageTransportSurfaceOverlayMac : public gfx::GLSurface,
                                        public ImageTransportSurface {
 public:
  ImageTransportSurfaceOverlayMac(GpuChannelManager* manager,
                                  GpuCommandBufferStub* stub,
                                  gfx::PluginWindowHandle handle);

  // GLSurface implementation
  bool Initialize() override;
  void Destroy() override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers() override;
  gfx::SwapResult PostSubBuffer(int x, int y, int width, int height) override;
  bool SupportsPostSubBuffer() override;
  gfx::Size GetSize() override;
  void* GetHandle() override;

  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            gfx::GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override;
  bool IsSurfaceless() const override;

  // ImageTransportSurface implementation
  void OnBufferPresented(
      const AcceleratedSurfaceMsg_BufferPresented_Params& params) override;
  void OnResize(gfx::Size pixel_size, float scale_factor) override;
  void SetLatencyInfo(const std::vector<ui::LatencyInfo>&) override;
  void WakeUpGpu() override;

 private:
  ~ImageTransportSurfaceOverlayMac() override;
  scoped_ptr<ImageTransportHelper> helper_;
  base::scoped_nsobject<CAContext> ca_context_;
  base::scoped_nsobject<CALayer> layer_;

  // A phony NSView handle used to identify this.
  gfx::AcceleratedWidget widget_;

  gfx::Size pixel_size_;
  float scale_factor_;
  std::vector<ui::LatencyInfo> latency_info_;

  // Weak pointer to the image provided when ScheduleOverlayPlane is called. Is
  // consumed and reset when SwapBuffers is called. For now, only one overlay
  // plane is supported.
  gfx::GLImage* pending_overlay_image_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_
