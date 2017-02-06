// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_CONTEXT_SUPPORT_H_
#define CC_TEST_TEST_CONTEXT_SUPPORT_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/client/context_support.h"

namespace gfx {
class Rect;
class RectF;
}

namespace cc {

class TestContextSupport : public gpu::ContextSupport {
 public:
  TestContextSupport();
  ~TestContextSupport() override;

  // gpu::ContextSupport implementation.
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       const base::Closure& callback) override;
  bool IsSyncTokenSignalled(const gpu::SyncToken& sync_token) override;
  void SignalQuery(uint32_t query, const base::Closure& callback) override;
  void SetAggressivelyFreeResources(bool aggressively_free_resources) override;
  void Swap() override;
  void SwapWithBounds(const std::vector<gfx::Rect>& rects) override;
  void PartialSwapBuffers(const gfx::Rect& sub_buffer) override;
  void CommitOverlayPlanes() override;
  void ScheduleOverlayPlane(int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            unsigned overlay_texture_id,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& uv_rect) override;
  uint64_t ShareGroupTracingGUID() const override;
  void SetErrorMessageCallback(
      const base::Callback<void(const char*, int32_t)>& callback) override;

  void CallAllSyncPointCallbacks();

  typedef base::Callback<void(int plane_z_order,
                              gfx::OverlayTransform plane_transform,
                              unsigned overlay_texture_id,
                              const gfx::Rect& display_bounds,
                              const gfx::RectF& crop_rect)>
      ScheduleOverlayPlaneCallback;
  void SetScheduleOverlayPlaneCallback(
      const ScheduleOverlayPlaneCallback& schedule_overlay_plane_callback);

  // If set true, callbacks triggering will be in a reverse order as SignalQuery
  // calls.
  void set_out_of_order_callbacks(bool out_of_order_callbacks) {
    out_of_order_callbacks_ = out_of_order_callbacks;
  }

 private:
  std::vector<base::Closure> sync_point_callbacks_;
  ScheduleOverlayPlaneCallback schedule_overlay_plane_callback_;
  bool out_of_order_callbacks_;

  base::WeakPtrFactory<TestContextSupport> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestContextSupport);
};

}  // namespace cc

#endif  // CC_TEST_TEST_CONTEXT_SUPPORT_H_
