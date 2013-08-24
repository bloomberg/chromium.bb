// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "cc/debug/rendering_stats.h"

namespace cc {

MainThreadRenderingStats::MainThreadRenderingStats()
  : animation_frame_count(0),
    screen_frame_count(0),
    commit_count(0),
    painted_pixel_count(0),
    recorded_pixel_count(0),
    image_gathering_count(0) {}

ImplThreadRenderingStats::ImplThreadRenderingStats()
    : screen_frame_count(0),
      dropped_frame_count(0),
      rasterized_pixel_count(0),
      impl_thread_scroll_count(0),
      main_thread_scroll_count(0),
      drawn_layer_count(0),
      missing_tile_count(0),
      deferred_image_decode_count(0),
      deferred_image_cache_hit_count(0),
      tile_analysis_count(0),
      solid_color_tile_analysis_count(0) {}

void RenderingStats::EnumerateFields(Enumerator* enumerator) const {
  enumerator->AddInt64("numAnimationFrames",
                       main_stats.animation_frame_count);
  enumerator->AddInt64("numFramesSentToScreen", main_stats.screen_frame_count +
                       impl_stats.screen_frame_count);
  enumerator->AddDouble("totalPaintTimeInSeconds",
                        main_stats.paint_time.InSecondsF());
  enumerator->AddDouble("totalRecordTimeInSeconds",
                        main_stats.record_time.InSecondsF());
  enumerator->AddDouble("totalCommitTimeInSeconds",
                        main_stats.commit_time.InSecondsF());
  enumerator->AddInt64("totalCommitCount", main_stats.commit_count);
  enumerator->AddInt64("totalPixelsPainted", main_stats.painted_pixel_count);
  enumerator->AddInt64("totalPixelsRecorded", main_stats.recorded_pixel_count);
  enumerator->AddInt64("totalImageGatheringCount",
                       main_stats.image_gathering_count);
  enumerator->AddDouble("totalImageGatheringTimeInSeconds",
                        main_stats.image_gathering_time.InSecondsF());
  enumerator->AddInt64("droppedFrameCount", impl_stats.dropped_frame_count);
  enumerator->AddDouble("totalRasterizeTimeInSeconds",
                        impl_stats.rasterize_time.InSecondsF());
  enumerator->AddDouble(
      "totalRasterizeTimeForNowBinsOnPendingTree",
      impl_stats.rasterize_time_for_now_bins_on_pending_tree.InSecondsF());
  enumerator->AddDouble("bestRasterizeTimeInSeconds",
                        impl_stats.best_rasterize_time.InSecondsF());
  enumerator->AddInt64("totalPixelsRasterized",
                       impl_stats.rasterized_pixel_count);
  enumerator->AddInt64("numImplThreadScrolls",
                       impl_stats.impl_thread_scroll_count);
  enumerator->AddInt64("numMainThreadScrolls",
                       impl_stats.main_thread_scroll_count);
  enumerator->AddInt64("numLayersDrawn", impl_stats.drawn_layer_count);
  enumerator->AddInt64("numMissingTiles", impl_stats.missing_tile_count);
  enumerator->AddInt64("totalDeferredImageDecodeCount",
                       impl_stats.deferred_image_decode_count);
  enumerator->AddInt64("totalTilesAnalyzed", impl_stats.tile_analysis_count);
  enumerator->AddInt64("solidColorTilesAnalyzed",
                       impl_stats.solid_color_tile_analysis_count);
  enumerator->AddInt64("totalDeferredImageCacheHitCount",
                       impl_stats.deferred_image_cache_hit_count);
  enumerator->AddDouble("totalDeferredImageDecodeTimeInSeconds",
                        impl_stats.deferred_image_decode_time.InSecondsF());
  enumerator->AddDouble("totalTileAnalysisTimeInSeconds",
                        impl_stats.tile_analysis_time.InSecondsF());
}

void MainThreadRenderingStats::IssueTraceEvent() const {
  TRACE_EVENT_INSTANT1("benchmark",
                       "MainThreadRenderingStats::IssueTraceEvent",
                       TRACE_EVENT_SCOPE_THREAD,
                       "data", AsTraceableData());
}

scoped_ptr<base::debug::ConvertableToTraceFormat>
MainThreadRenderingStats::AsTraceableData() const {
  scoped_ptr<base::DictionaryValue> record_data(new base::DictionaryValue());
  record_data->SetInteger("animation_frame_count",
                          animation_frame_count);
  record_data->SetInteger("screen_frame_count",
                          screen_frame_count);
  record_data->SetDouble("paint_time",
                         paint_time.InSecondsF());
  record_data->SetDouble("record_time",
                         record_time.InSecondsF());
  record_data->SetDouble("commit_time",
                         commit_time.InSecondsF());
  record_data->SetInteger("commit_count",
                          commit_count);
  record_data->SetInteger("painted_pixel_count",
                          painted_pixel_count);
  record_data->SetInteger("recorded_pixel_count",
                          recorded_pixel_count);
  record_data->SetInteger("image_gathering_count",
                          image_gathering_count);
  record_data->SetDouble("image_gathering_time",
                         image_gathering_time.InSecondsF());
  return TracedValue::FromValue(record_data.release());
}

void ImplThreadRenderingStats::IssueTraceEvent() const {
  TRACE_EVENT_INSTANT1("benchmark",
                       "ImplThreadRenderingStats::IssueTraceEvent",
                       TRACE_EVENT_SCOPE_THREAD,
                       "data", AsTraceableData());
}

scoped_ptr<base::debug::ConvertableToTraceFormat>
ImplThreadRenderingStats::AsTraceableData() const {
  scoped_ptr<base::DictionaryValue> record_data(new base::DictionaryValue());
  record_data->SetInteger("screen_frame_count",
                          screen_frame_count);
  record_data->SetInteger("dropped_frame_count",
                          dropped_frame_count);
  record_data->SetDouble("rasterize_time",
                         rasterize_time.InSecondsF());
  record_data->SetDouble(
      "rasterize_time_for_now_bins_on_pending_tree",
      rasterize_time_for_now_bins_on_pending_tree.InSecondsF());
  record_data->SetDouble("best_rasterize_time",
                         best_rasterize_time.InSecondsF());
  record_data->SetInteger("rasterized_pixel_count",
                          rasterized_pixel_count);
  record_data->SetInteger("impl_thread_scroll_count",
                          impl_thread_scroll_count);
  record_data->SetInteger("main_thread_scroll_count",
                          main_thread_scroll_count);
  record_data->SetInteger("drawn_layer_count",
                          drawn_layer_count);
  record_data->SetInteger("missing_tile_count",
                          missing_tile_count);
  record_data->SetInteger("deferred_image_decode_count",
                          deferred_image_decode_count);
  record_data->SetInteger("deferred_image_cache_hit_count",
                          deferred_image_cache_hit_count);
  record_data->SetInteger("tile_analysis_count",
                          tile_analysis_count);
  record_data->SetInteger("solid_color_tile_analysis_count",
                          solid_color_tile_analysis_count);
  record_data->SetDouble("deferred_image_decode_time",
                         deferred_image_decode_time.InSecondsF());
  record_data->SetDouble("tile_analysis_time",
                         tile_analysis_time.InSecondsF());
  return TracedValue::FromValue(record_data.release());
}


void MainThreadRenderingStats::Add(const MainThreadRenderingStats& other) {
  animation_frame_count += other.animation_frame_count;
  screen_frame_count += other.screen_frame_count;
  paint_time += other.paint_time;
  record_time += other.record_time;
  commit_time += other.commit_time;
  commit_count += other.commit_count;
  painted_pixel_count += other.painted_pixel_count;
  recorded_pixel_count += other.recorded_pixel_count;
  image_gathering_count += other.image_gathering_count;
  image_gathering_time += other.image_gathering_time;
}

void ImplThreadRenderingStats::Add(const ImplThreadRenderingStats& other) {
  screen_frame_count += other.screen_frame_count;
  dropped_frame_count += other.dropped_frame_count;
  rasterize_time += other.rasterize_time;
  rasterize_time_for_now_bins_on_pending_tree +=
      other.rasterize_time_for_now_bins_on_pending_tree;
  best_rasterize_time += other.best_rasterize_time;
  rasterized_pixel_count += other.rasterized_pixel_count;
  impl_thread_scroll_count += other.impl_thread_scroll_count;
  main_thread_scroll_count += other.main_thread_scroll_count;
  drawn_layer_count += other.drawn_layer_count;
  missing_tile_count += other.missing_tile_count;
  deferred_image_decode_count += other.deferred_image_decode_count;
  deferred_image_cache_hit_count += other.deferred_image_cache_hit_count;
  deferred_image_decode_time += other.deferred_image_decode_time;
  tile_analysis_count += other.tile_analysis_count;
  solid_color_tile_analysis_count += other.solid_color_tile_analysis_count;
  tile_analysis_time += other.tile_analysis_time;
}

void RenderingStats::Add(const RenderingStats& other) {
  main_stats.Add(other.main_stats);
  impl_stats.Add(other.impl_stats);
}

}  // namespace cc
