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

RenderingStatsInstrumentation::~RenderingStatsInstrumentation() {}

RenderingStats RenderingStatsInstrumentation::GetRenderingStats() {
  base::AutoLock scoped_lock(lock_);
  RenderingStats rendering_stats;
  rendering_stats.main_stats = main_stats_accu_;
  rendering_stats.main_stats.Add(main_stats_);
  rendering_stats.impl_stats = impl_stats_accu_;
  rendering_stats.impl_stats.Add(impl_stats_);
  return rendering_stats;
}

void RenderingStatsInstrumentation::AccumulateAndClearMainThreadStats() {
  main_stats_accu_.Add(main_stats_);
  main_stats_ = MainThreadRenderingStats();
}

void RenderingStatsInstrumentation::AccumulateAndClearImplThreadStats() {
  impl_stats_accu_.Add(impl_stats_);
  impl_stats_ = ImplThreadRenderingStats();
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

void RenderingStatsInstrumentation::IncrementAnimationFrameCount() {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  main_stats_.animation_frame_count++;
}

void RenderingStatsInstrumentation::IncrementScreenFrameCount(
    int64 count, bool main_thread) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  if (main_thread)
    main_stats_.screen_frame_count += count;
  else
    impl_stats_.screen_frame_count += count;
}

void RenderingStatsInstrumentation::IncrementDroppedFrameCount(int64 count) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_stats_.dropped_frame_count += count;
}

void RenderingStatsInstrumentation::AddCommit(base::TimeDelta duration) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  main_stats_.commit_time += duration;
  main_stats_.commit_count++;
}

void RenderingStatsInstrumentation::AddPaint(base::TimeDelta duration,
                                             int64 pixels) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  main_stats_.paint_time += duration;
  main_stats_.painted_pixel_count += pixels;
}

void RenderingStatsInstrumentation::AddRecord(base::TimeDelta duration,
                                              int64 pixels) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  main_stats_.record_time += duration;
  main_stats_.recorded_pixel_count += pixels;
}

void RenderingStatsInstrumentation::AddRaster(base::TimeDelta total_duration,
                                              base::TimeDelta best_duration,
                                              int64 pixels,
                                              bool is_in_pending_tree_now_bin) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_stats_.rasterize_time += total_duration;
  impl_stats_.best_rasterize_time += best_duration;
  impl_stats_.rasterized_pixel_count += pixels;

  if (is_in_pending_tree_now_bin) {
    impl_stats_.rasterize_time_for_now_bins_on_pending_tree +=
        total_duration;
  }
}

void RenderingStatsInstrumentation::IncrementImplThreadScrolls() {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_stats_.impl_thread_scroll_count++;
}

void RenderingStatsInstrumentation::IncrementMainThreadScrolls() {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_stats_.main_thread_scroll_count++;
}

void RenderingStatsInstrumentation::AddLayersDrawn(int64 amount) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_stats_.drawn_layer_count += amount;
}

void RenderingStatsInstrumentation::AddMissingTiles(int64 amount) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_stats_.missing_tile_count += amount;
}

void RenderingStatsInstrumentation::AddDeferredImageDecode(
    base::TimeDelta duration) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_stats_.deferred_image_decode_time += duration;
  impl_stats_.deferred_image_decode_count++;
}

void RenderingStatsInstrumentation::AddImageGathering(
    base::TimeDelta duration) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  main_stats_.image_gathering_time += duration;
  main_stats_.image_gathering_count++;
}

void RenderingStatsInstrumentation::IncrementDeferredImageCacheHitCount() {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_stats_.deferred_image_cache_hit_count++;
}

void RenderingStatsInstrumentation::AddAnalysisResult(
    base::TimeDelta duration,
    bool is_solid_color) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_stats_.tile_analysis_count++;
  impl_stats_.tile_analysis_time += duration;
  if (is_solid_color)
    impl_stats_.solid_color_tile_analysis_count++;
}

}  // namespace cc
