// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_context_support.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace cc {

TestContextSupport::TestContextSupport()
    : last_swap_type_(NO_SWAP),
      weak_ptr_factory_(this) {
}

TestContextSupport::~TestContextSupport() {}

void TestContextSupport::SignalSyncPoint(uint32 sync_point,
                                         const base::Closure& callback) {
  sync_point_callbacks_.push_back(callback);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&TestContextSupport::CallAllSyncPointCallbacks,
                 weak_ptr_factory_.GetWeakPtr()));
}

void TestContextSupport::SignalQuery(uint32 query,
                                     const base::Closure& callback) {
  sync_point_callbacks_.push_back(callback);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&TestContextSupport::CallAllSyncPointCallbacks,
                 weak_ptr_factory_.GetWeakPtr()));
}

void TestContextSupport::SetSurfaceVisible(bool visible) {
  if (!set_visible_callback_.is_null()) {
    set_visible_callback_.Run(visible);
  }
}

void TestContextSupport::CallAllSyncPointCallbacks() {
  for (size_t i = 0; i < sync_point_callbacks_.size(); ++i) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, sync_point_callbacks_[i]);
  }
  sync_point_callbacks_.clear();
}

void TestContextSupport::SetSurfaceVisibleCallback(
    const SurfaceVisibleCallback& set_visible_callback) {
  set_visible_callback_ = set_visible_callback;
}

void TestContextSupport::SetScheduleOverlayPlaneCallback(
    const ScheduleOverlayPlaneCallback& schedule_overlay_plane_callback) {
  schedule_overlay_plane_callback_ = schedule_overlay_plane_callback;
}

void TestContextSupport::Swap() {
  last_swap_type_ = SWAP;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&TestContextSupport::OnSwapBuffersComplete,
                            weak_ptr_factory_.GetWeakPtr()));
}

uint32 TestContextSupport::InsertFutureSyncPointCHROMIUM() {
  NOTIMPLEMENTED();
  return 0;
}

void TestContextSupport::RetireSyncPointCHROMIUM(uint32 sync_point) {
  NOTIMPLEMENTED();
}

void TestContextSupport::PartialSwapBuffers(const gfx::Rect& sub_buffer) {
  last_swap_type_ = PARTIAL_SWAP;
  last_partial_swap_rect_ = sub_buffer;
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&TestContextSupport::OnSwapBuffersComplete,
                            weak_ptr_factory_.GetWeakPtr()));
}

void TestContextSupport::ScheduleOverlayPlane(
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    unsigned overlay_texture_id,
    const gfx::Rect& display_bounds,
    const gfx::RectF& uv_rect) {
  if (!schedule_overlay_plane_callback_.is_null()) {
    schedule_overlay_plane_callback_.Run(plane_z_order,
                                         plane_transform,
                                         overlay_texture_id,
                                         display_bounds,
                                         uv_rect);
  }
}

void TestContextSupport::SetSwapBuffersCompleteCallback(
    const base::Closure& callback) {
  swap_buffers_complete_callback_ = callback;
}

void TestContextSupport::OnSwapBuffersComplete() {
  if (!swap_buffers_complete_callback_.is_null())
    swap_buffers_complete_callback_.Run();
}

}  // namespace cc
