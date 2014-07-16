// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_CONTEXT_SUPPORT_H_
#define CC_TEST_TEST_CONTEXT_SUPPORT_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/client/context_support.h"

namespace cc {

class TestContextSupport : public gpu::ContextSupport {
 public:
  TestContextSupport();
  virtual ~TestContextSupport();

  // gpu::ContextSupport implementation.
  virtual void SignalSyncPoint(uint32 sync_point,
                               const base::Closure& callback) OVERRIDE;
  virtual void SignalQuery(uint32 query,
                           const base::Closure& callback) OVERRIDE;
  virtual void SetSurfaceVisible(bool visible) OVERRIDE;
  virtual void Swap() OVERRIDE;
  virtual void PartialSwapBuffers(const gfx::Rect& sub_buffer) OVERRIDE;
  virtual uint32 InsertFutureSyncPointCHROMIUM() OVERRIDE;
  virtual void RetireSyncPointCHROMIUM(uint32 sync_point) OVERRIDE;
  virtual void SetSwapBuffersCompleteCallback(
      const base::Closure& callback) OVERRIDE;
  virtual void ScheduleOverlayPlane(int plane_z_order,
                                    gfx::OverlayTransform plane_transform,
                                    unsigned overlay_texture_id,
                                    const gfx::Rect& display_bounds,
                                    const gfx::RectF& uv_rect) OVERRIDE;

  void CallAllSyncPointCallbacks();

  typedef base::Callback<void(bool visible)> SurfaceVisibleCallback;
  void SetSurfaceVisibleCallback(
      const SurfaceVisibleCallback& set_visible_callback);

  typedef base::Callback<void(int plane_z_order,
                              gfx::OverlayTransform plane_transform,
                              unsigned overlay_texture_id,
                              const gfx::Rect& display_bounds,
                              const gfx::RectF& crop_rect)>
      ScheduleOverlayPlaneCallback;
  void SetScheduleOverlayPlaneCallback(
      const ScheduleOverlayPlaneCallback& schedule_overlay_plane_callback);

  enum SwapType {
    NO_SWAP,
    SWAP,
    PARTIAL_SWAP
  };

  SwapType last_swap_type() const { return last_swap_type_; }
  gfx::Rect last_partial_swap_rect() const {
    return last_partial_swap_rect_;
  }

 private:
  void OnSwapBuffersComplete();

  std::vector<base::Closure> sync_point_callbacks_;
  SurfaceVisibleCallback set_visible_callback_;
  ScheduleOverlayPlaneCallback schedule_overlay_plane_callback_;

  base::Closure swap_buffers_complete_callback_;

  SwapType last_swap_type_;
  gfx::Rect last_partial_swap_rect_;

  base::WeakPtrFactory<TestContextSupport> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestContextSupport);
};

}  // namespace cc

#endif  // CC_TEST_TEST_CONTEXT_SUPPORT_H_
