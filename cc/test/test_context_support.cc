// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_context_support.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace cc {

TestContextSupport::TestContextSupport()
    : out_of_order_callbacks_(false), weak_ptr_factory_(this) {}

TestContextSupport::~TestContextSupport() {}

void TestContextSupport::SignalSyncToken(const gpu::SyncToken& sync_token,
                                         const base::Closure& callback) {
  sync_point_callbacks_.push_back(callback);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&TestContextSupport::CallAllSyncPointCallbacks,
                            weak_ptr_factory_.GetWeakPtr()));
}

bool TestContextSupport::IsSyncTokenSignalled(
    const gpu::SyncToken& sync_token) {
  return true;
}

void TestContextSupport::SignalQuery(uint32_t query,
                                     const base::Closure& callback) {
  sync_point_callbacks_.push_back(callback);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&TestContextSupport::CallAllSyncPointCallbacks,
                            weak_ptr_factory_.GetWeakPtr()));
}

void TestContextSupport::SetAggressivelyFreeResources(
    bool aggressively_free_resources) {}

void TestContextSupport::CallAllSyncPointCallbacks() {
  size_t size = sync_point_callbacks_.size();
  if (out_of_order_callbacks_) {
    for (size_t i = size; i > 0; --i) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, sync_point_callbacks_[i - 1]);
    }
  } else {
    for (size_t i = 0; i < size; ++i) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                    sync_point_callbacks_[i]);
    }
  }
  sync_point_callbacks_.clear();
}

void TestContextSupport::SetScheduleOverlayPlaneCallback(
    const ScheduleOverlayPlaneCallback& schedule_overlay_plane_callback) {
  schedule_overlay_plane_callback_ = schedule_overlay_plane_callback;
}

void TestContextSupport::Swap() {}

void TestContextSupport::SwapWithBounds(const std::vector<gfx::Rect>& rects) {}

void TestContextSupport::PartialSwapBuffers(const gfx::Rect& sub_buffer) {}

void TestContextSupport::CommitOverlayPlanes() {}

void TestContextSupport::ScheduleOverlayPlane(
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    unsigned overlay_texture_id,
    const gfx::Rect& display_bounds,
    const gfx::RectF& uv_rect) {
  if (!schedule_overlay_plane_callback_.is_null()) {
    schedule_overlay_plane_callback_.Run(plane_z_order, plane_transform,
                                         overlay_texture_id, display_bounds,
                                         uv_rect);
  }
}

uint64_t TestContextSupport::ShareGroupTracingGUID() const {
  NOTIMPLEMENTED();
  return 0;
}

void TestContextSupport::SetErrorMessageCallback(
    const base::Callback<void(const char*, int32_t)>& callback) {}

}  // namespace cc
