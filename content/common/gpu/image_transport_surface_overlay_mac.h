// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_
#define CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_

#include <list>
#include <vector>

#include "base/memory/linked_ptr.h"
#import "base/mac/scoped_nsobject.h"
#include "base/timer/timer.h"
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
  bool SetBackbufferAllocation(bool allocated) override;
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

  // ui::GpuSwitchingObserver implementation.
  void OnGpuSwitched() override;

 private:
  class PendingSwap;
  class OverlayPlane;

  ~ImageTransportSurfaceOverlayMac() override;

  gfx::SwapResult SwapBuffersInternal(const gfx::Rect& pixel_damage_rect);

  void UpdateRootAndPartialDamagePlanes(
      const linked_ptr<OverlayPlane>& new_root_plane,
      const gfx::RectF& dip_damage_rect);
  void UpdateOverlayPlanes(
      const std::vector<linked_ptr<OverlayPlane>>& new_overlay_planes);
  void UpdateCALayerTree();

  // Returns true if the front of |pending_swaps_| has completed, or has timed
  // out by |now|.
  bool IsFirstPendingSwapReadyToDisplay(
    const base::TimeTicks& now);
  // Sets the CALayer contents to the IOSurface for the front of
  // |pending_swaps_|, and removes it from the queue.
  void DisplayFirstPendingSwapImmediately();
  // Force that all of |pending_swaps_| displayed immediately, and the list be
  // cleared.
  void DisplayAndClearAllPendingSwaps();
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
  bool use_remote_layer_api_;
  base::scoped_nsobject<CAContext> ca_context_;
  base::scoped_nsobject<CALayer> ca_root_layer_;

  gfx::Size pixel_size_;
  float scale_factor_;
  std::vector<ui::LatencyInfo> latency_info_;

  // The renderer ID that all contexts made current to this surface should be
  // targeting.
  GLint gl_renderer_id_;

  // Planes that have been scheduled, but have not had a subsequent SwapBuffers
  // call made yet.
  linked_ptr<OverlayPlane> pending_root_plane_;
  std::vector<linked_ptr<OverlayPlane>> pending_overlay_planes_;

  // A queue of all frames that have been created by SwapBuffersInternal but
  // have not yet been displayed. This queue is checked at the beginning of
  // every swap and also by a callback.
  std::deque<linked_ptr<PendingSwap>> pending_swaps_;

  // The planes that are currently being displayed on the screen.
  linked_ptr<OverlayPlane> current_root_plane_;
  std::list<linked_ptr<OverlayPlane>> current_partial_damage_planes_;
  std::list<linked_ptr<OverlayPlane>> current_overlay_planes_;

  // The time of the last swap was issued. If this is more than two vsyncs, then
  // use the simpler non-smooth animation path.
  base::TimeTicks last_swap_time_;

  // The vsync information provided by the browser.
  bool vsync_parameters_valid_;
  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_;

  base::Timer display_pending_swap_timer_;
  base::WeakPtrFactory<ImageTransportSurfaceOverlayMac> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_OVERLAY_MAC_H_
