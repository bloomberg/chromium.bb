// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_
#define CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_

#include <deque>

#include "base/memory/linked_ptr.h"
#import "base/mac/scoped_nsobject.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/image_transport_surface.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_switching_observer.h"

@class CAContext;
@class CALayer;

namespace content {

class ImageTransportSurfaceOverlayMac : public gfx::GLSurface,
                                        public ImageTransportSurface,
                                        public ui::GpuSwitchingObserver {
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
  bool OnMakeCurrent(gfx::GLContext* context) override;
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

  // ui::GpuSwitchingObserver implementation.
  void OnGpuSwitched() override;

 private:
  class PendingSwap;

  ~ImageTransportSurfaceOverlayMac() override;

  gfx::SwapResult SwapBuffersInternal(const gfx::Rect& pixel_damage_rect);

  // Returns true if the front of |pending_swaps_| has completed, or has timed
  // out by |now|.
  bool IsFirstPendingSwapReadyToDisplay(
    const base::TimeTicks& now);
  // Sets the CALayer contents to the IOSurface for the front of
  // |pending_swaps_|, and removes it from the queue.
  void DisplayFirstPendingSwapImmediately();
  // Force that all of |pending_swaps_| displayed immediately, and the list be
  // cleared.
  void FinishAllPendingSwaps();
  // Callback issued during the next vsync period ofter a SwapBuffers call,
  // to check if the swap is completed, and display the frame. Note that if
  // another SwapBuffers happens before this callback, the pending swap will
  // be tested at that time, too.
  void CheckPendingSwapsCallback();
  // Function to post the above callback. The argument |now| is passed as an
  // argument to avoid redundant calls to base::TimeTicks::Now.
  void PostCheckPendingSwapsCallbackIfNeeded(const base::TimeTicks& now);

  // Return the time of |interval_fraction| of the way through the next
  // vsync period that starts after |from|. If the vsync parameters are not
  // valid then return |from|.
  base::TimeTicks GetNextVSyncTimeAfter(
      const base::TimeTicks& from, double interval_fraction);

  scoped_ptr<ImageTransportHelper> helper_;
  base::scoped_nsobject<CAContext> ca_context_;
  base::scoped_nsobject<CALayer> layer_;
  base::scoped_nsobject<CALayer> partial_damage_layer_;

  gfx::Size pixel_size_;
  float scale_factor_;
  std::vector<ui::LatencyInfo> latency_info_;

  // The renderer ID that all contexts made current to this surface should be
  // targeting.
  GLint gl_renderer_id_;

  // Weak pointer to the image provided when ScheduleOverlayPlane is called. Is
  // consumed and reset when SwapBuffers is called. For now, only one overlay
  // plane is supported.
  gfx::GLImage* pending_overlay_image_;

  // A queue of all frames that have been created by SwapBuffersInternal but
  // have not yet been displayed. This queue is checked at the beginning of
  // every swap and also by a callback.
  std::deque<linked_ptr<PendingSwap>> pending_swaps_;

  // The union of the damage rects of SwapBuffersInternal since the last
  // non-partial swap.
  gfx::Rect accumulated_partial_damage_pixel_rect_;

  // The vsync information provided by the browser.
  bool vsync_parameters_valid_;
  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_;

  // True if there is a pending call to CheckPendingSwapsCallback posted.
  bool has_pending_callback_;

  base::WeakPtrFactory<ImageTransportSurfaceOverlayMac> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_
