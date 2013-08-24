// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_RENDERING_STATS_H_
#define CC_DEBUG_RENDERING_STATS_H_

#include "base/basictypes.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/debug/traced_value.h"

namespace cc {

// In conjunction with EnumerateFields, this allows the embedder to
// enumerate the values in this structure without
// having to embed references to its specific member variables. This
// simplifies the addition of new fields to this type.
class RenderingStatsEnumerator {
 public:
  virtual void AddInt64(const char* name, int64 value) = 0;
  virtual void AddDouble(const char* name, double value) = 0;
  virtual void AddInt(const char* name, int value) = 0;
  virtual void AddTimeDeltaInSecondsF(const char* name,
                                      const base::TimeDelta& value) = 0;

 protected:
  virtual ~RenderingStatsEnumerator() {}
};

struct CC_EXPORT MainThreadRenderingStats {
  // Note: when adding new members, please remember to update EnumerateFields
  // and Add in rendering_stats.cc.

  int64 animation_frame_count;
  int64 screen_frame_count;
  base::TimeDelta paint_time;
  base::TimeDelta record_time;
  base::TimeDelta commit_time;
  int64 commit_count;
  int64 painted_pixel_count;
  int64 recorded_pixel_count;
  int64 image_gathering_count;
  base::TimeDelta image_gathering_time;

  MainThreadRenderingStats();
  void IssueTraceEvent() const;
  scoped_ptr<base::debug::ConvertableToTraceFormat> AsTraceableData() const;
  void Add(const MainThreadRenderingStats& other);
};

struct CC_EXPORT ImplThreadRenderingStats {
  // Note: when adding new members, please remember to update EnumerateFields
  // and Add in rendering_stats.cc.

  int64 screen_frame_count;
  int64 dropped_frame_count;
  base::TimeDelta rasterize_time;
  base::TimeDelta rasterize_time_for_now_bins_on_pending_tree;
  base::TimeDelta best_rasterize_time;
  int64 rasterized_pixel_count;
  int64 impl_thread_scroll_count;
  int64 main_thread_scroll_count;
  int64 drawn_layer_count;
  int64 missing_tile_count;
  int64 deferred_image_decode_count;
  int64 deferred_image_cache_hit_count;
  int64 tile_analysis_count;
  int64 solid_color_tile_analysis_count;
  base::TimeDelta deferred_image_decode_time;
  base::TimeDelta tile_analysis_time;

  ImplThreadRenderingStats();
  void IssueTraceEvent() const;
  scoped_ptr<base::debug::ConvertableToTraceFormat> AsTraceableData() const;
  void Add(const ImplThreadRenderingStats& other);
};

struct CC_EXPORT RenderingStats {
  typedef RenderingStatsEnumerator Enumerator;

  MainThreadRenderingStats main_stats;
  ImplThreadRenderingStats impl_stats;

  // Outputs the fields in this structure to the provided enumerator.
  void EnumerateFields(Enumerator* enumerator) const;

  // Add fields of |other| to the fields in this structure.
  void Add(const RenderingStats& other);
};

}  // namespace cc

#endif  // CC_DEBUG_RENDERING_STATS_H_
