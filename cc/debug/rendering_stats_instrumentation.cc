// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/rendering_stats_instrumentation.h"

namespace cc {

// static
scoped_ptr<RenderingStatsInstrumentation>
    RenderingStatsInstrumentation::Create() {
  return make_scoped_ptr(new RenderingStatsInstrumentation());
}

RenderingStatsInstrumentation::RenderingStatsInstrumentation()
    : record_rendering_stats_(false) {
}

RenderingStats RenderingStatsInstrumentation::GetRenderingStats() {
  base::AutoLock scoped_lock(lock_);
  return rendering_stats_;
}

base::TimeTicks RenderingStatsInstrumentation::StartRecording() const {
  if (record_rendering_stats_)
    return base::TimeTicks::HighResNow();
  return base::TimeTicks();
}

base::TimeDelta RenderingStatsInstrumentation::EndRecording(
    base::TimeTicks start_time) const {
  if (!start_time.is_null())
    return base::TimeTicks::HighResNow() - start_time;
  return base::TimeDelta();
}

void RenderingStatsInstrumentation::AddStats(const RenderingStats& other) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  rendering_stats_.Add(other);
}

void RenderingStatsInstrumentation::IncrementAnimationFrameCount() {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  rendering_stats_.numAnimationFrames++;
}

void RenderingStatsInstrumentation::SetScreenFrameCount(int64 count) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  rendering_stats_.numFramesSentToScreen = count;
}

void RenderingStatsInstrumentation::SetDroppedFrameCount(int64 count) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  rendering_stats_.droppedFrameCount = count;
}

void RenderingStatsInstrumentation::AddCommit(base::TimeDelta duration) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  rendering_stats_.totalCommitTime += duration;
  rendering_stats_.totalCommitCount++;
}

void RenderingStatsInstrumentation::AddPaint(base::TimeDelta duration,
                                             int64 pixels) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  rendering_stats_.totalPaintTime += duration;
  rendering_stats_.totalPixelsPainted += pixels;
}

void RenderingStatsInstrumentation::AddRaster(base::TimeDelta duration,
                                              int64 pixels,
                                              bool is_in_pending_tree_now_bin) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  rendering_stats_.totalRasterizeTime += duration;
  rendering_stats_.totalPixelsRasterized += pixels;

  if (is_in_pending_tree_now_bin)
    rendering_stats_.totalRasterizeTimeForNowBinsOnPendingTree += duration;
}

void RenderingStatsInstrumentation::IncrementImplThreadScrolls() {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  rendering_stats_.numImplThreadScrolls++;
}

void RenderingStatsInstrumentation::IncrementMainThreadScrolls() {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  rendering_stats_.numMainThreadScrolls++;
}

void RenderingStatsInstrumentation::AddLayersDrawn(int64 amount) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  rendering_stats_.numLayersDrawn += amount;
}

void RenderingStatsInstrumentation::AddMissingTiles(int64 amount) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  rendering_stats_.numMissingTiles += amount;
}

void RenderingStatsInstrumentation::AddDeferredImageDecode(
    base::TimeDelta duration) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  rendering_stats_.totalDeferredImageDecodeTime += duration;
  rendering_stats_.totalDeferredImageDecodeCount++;
}

void RenderingStatsInstrumentation::AddImageGathering(
    base::TimeDelta duration) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  rendering_stats_.totalImageGatheringTime += duration;
  rendering_stats_.totalImageGatheringCount++;
}

void RenderingStatsInstrumentation::IncrementDeferredImageCacheHitCount() {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  rendering_stats_.totalDeferredImageCacheHitCount++;
}

}  // namespace cc
