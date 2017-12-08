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

TestContextSupport::~TestContextSupport() = default;

void TestContextSupport::FlushPendingWork() {}

void TestContextSupport::SignalSyncToken(const gpu::SyncToken& sync_token,
                                         base::OnceClosure callback) {
  sync_point_callbacks_.push_back(std::move(callback));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TestContextSupport::CallAllSyncPointCallbacks,
                                weak_ptr_factory_.GetWeakPtr()));
}

bool TestContextSupport::IsSyncTokenSignaled(const gpu::SyncToken& sync_token) {
  return true;
}

void TestContextSupport::SignalQuery(uint32_t query,
                                     base::OnceClosure callback) {
  sync_point_callbacks_.push_back(std::move(callback));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TestContextSupport::CallAllSyncPointCallbacks,
                                weak_ptr_factory_.GetWeakPtr()));
}

void TestContextSupport::SetAggressivelyFreeResources(
    bool aggressively_free_resources) {}

void TestContextSupport::CallAllSyncPointCallbacks() {
  size_t size = sync_point_callbacks_.size();
  if (out_of_order_callbacks_) {
    for (size_t i = size; i > 0; --i) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(sync_point_callbacks_[i - 1]));
    }
  } else {
    for (size_t i = 0; i < size; ++i) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(sync_point_callbacks_[i]));
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
    base::RepeatingCallback<void(const char*, int32_t)> callback) {}

void TestContextSupport::SetSnapshotRequested() {}

bool TestContextSupport::ThreadSafeShallowLockDiscardableTexture(
    uint32_t texture_id) {
  NOTIMPLEMENTED();
  return false;
}
void TestContextSupport::CompleteLockDiscardableTexureOnContextThread(
    uint32_t texture_id) {}

gpu::TransferCacheEntryId TestContextSupport::CreateTransferCacheEntry(
    const ClientTransferCacheEntry& entry) {
  NOTIMPLEMENTED();
  return gpu::TransferCacheEntryId();
}
bool TestContextSupport::ThreadsafeLockTransferCacheEntry(
    gpu::TransferCacheEntryId id) {
  NOTIMPLEMENTED();
  return false;
}
void TestContextSupport::UnlockTransferCacheEntry(
    gpu::TransferCacheEntryId id) {
  NOTIMPLEMENTED();
}
void TestContextSupport::DeleteTransferCacheEntry(
    gpu::TransferCacheEntryId id) {
  NOTIMPLEMENTED();
}

}  // namespace cc
