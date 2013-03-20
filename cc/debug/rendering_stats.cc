// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/rendering_stats.h"

namespace cc {

RenderingStats::RenderingStats()
    : animation_frame_count(0),
      screen_frame_count(0),
      dropped_frame_count(0),
      total_commit_count(0),
      total_pixels_painted(0),
      total_pixels_rasterized(0),
      num_impl_thread_scrolls(0),
      num_main_thread_scrolls(0),
      num_layers_drawn(0),
      num_missing_tiles(0),
      total_deferred_image_decode_count(0),
      total_deferred_image_cache_hit_count(0),
      total_image_gathering_count(0) {}

void RenderingStats::EnumerateFields(Enumerator* enumerator) const {
  enumerator->AddInt64("animation_frame_count", animation_frame_count);
  enumerator->AddInt64("screen_frame_count", screen_frame_count);
  enumerator->AddInt64("dropped_frame_count", dropped_frame_count);
  enumerator->AddDouble("total_paint_timeInSeconds",
                        total_paint_time.InSecondsF());
  enumerator->AddDouble("total_rasterize_timeInSeconds",
                        total_rasterize_time.InSecondsF());
  enumerator->AddDouble(
      "total_rasterize_time_for_now_bins_on_pending_tree",
      total_rasterize_time_for_now_bins_on_pending_tree.InSecondsF());
  enumerator->AddDouble("total_commit_timeInSeconds",
                        total_commit_time.InSecondsF());
  enumerator->AddInt64("total_commit_count", total_commit_count);
  enumerator->AddInt64("total_pixels_painted", total_pixels_painted);
  enumerator->AddInt64("total_pixels_rasterized", total_pixels_rasterized);
  enumerator->AddInt64("num_impl_thread_scrolls", num_impl_thread_scrolls);
  enumerator->AddInt64("num_main_thread_scrolls", num_main_thread_scrolls);
  enumerator->AddInt64("num_layers_drawn", num_layers_drawn);
  enumerator->AddInt64("num_missing_tiles", num_missing_tiles);
  enumerator->AddInt64("total_deferred_image_decode_count",
                       total_deferred_image_decode_count);
  enumerator->AddInt64("total_deferred_image_cache_hit_count",
                       total_deferred_image_cache_hit_count);
  enumerator->AddInt64("total_image_gathering_count",
                       total_image_gathering_count);
  enumerator->AddDouble("total_deferred_image_decode_timeInSeconds",
                        total_deferred_image_decode_time.InSecondsF());
  enumerator->AddDouble("total_image_gathering_timeInSeconds",
                        total_image_gathering_time.InSecondsF());
}

void RenderingStats::Add(const RenderingStats& other) {
  animation_frame_count += other.animation_frame_count;
  screen_frame_count += other.screen_frame_count;
  dropped_frame_count += other.dropped_frame_count;
  total_paint_time += other.total_paint_time;
  total_rasterize_time += other.total_rasterize_time;
  total_rasterize_time_for_now_bins_on_pending_tree +=
      other.total_rasterize_time_for_now_bins_on_pending_tree;
  total_commit_time += other.total_commit_time;
  total_commit_count += other.total_commit_count;
  total_pixels_painted += other.total_pixels_painted;
  total_pixels_rasterized += other.total_pixels_rasterized;
  num_impl_thread_scrolls += other.num_impl_thread_scrolls;
  num_main_thread_scrolls += other.num_main_thread_scrolls;
  num_layers_drawn += other.num_layers_drawn;
  num_missing_tiles += other.num_missing_tiles;
  total_deferred_image_decode_count += other.total_deferred_image_decode_count;
  total_deferred_image_cache_hit_count +=
      other.total_deferred_image_cache_hit_count;
  total_image_gathering_count += other.total_image_gathering_count;
  total_deferred_image_decode_time += other.total_deferred_image_decode_time;
  total_image_gathering_time += other.total_image_gathering_time;
}

}  // namespace cc
