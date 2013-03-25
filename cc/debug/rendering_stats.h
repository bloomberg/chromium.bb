// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_RENDERING_STATS_H_
#define CC_DEBUG_RENDERING_STATS_H_

#include "base/basictypes.h"
#include "base/time.h"
#include "cc/base/cc_export.h"

namespace cc {

struct CC_EXPORT RenderingStats {
  int64 animation_frame_count;
  int64 screen_frame_count;
  int64 dropped_frame_count;
  base::TimeDelta total_paint_time;
  base::TimeDelta total_rasterize_time;
  base::TimeDelta total_rasterize_time_for_now_bins_on_pending_tree;
  base::TimeDelta total_commit_time;
  int64 total_commit_count;
  int64 total_pixels_painted;
  int64 total_pixels_rasterized;
  int64 num_impl_thread_scrolls;
  int64 num_main_thread_scrolls;
  int64 num_layers_drawn;
  int64 num_missing_tiles;
  int64 total_deferred_image_decode_count;
  int64 total_deferred_image_cache_hit_count;
  int64 total_image_gathering_count;
  base::TimeDelta total_deferred_image_decode_time;
  base::TimeDelta total_image_gathering_time;
  // Note: when adding new members, please remember to update EnumerateFields
  // and Add in rendering_stats.cc.

  RenderingStats();

  // In conjunction with EnumerateFields, this allows the embedder to
  // enumerate the values in this structure without
  // having to embed references to its specific member variables. This
  // simplifies the addition of new fields to this type.
  class Enumerator {
   public:
    virtual void AddInt64(const char* name, int64 value) = 0;
    virtual void AddDouble(const char* name, double value) = 0;
    virtual void AddInt(const char* name, int value) = 0;
    virtual void AddTimeDeltaInSecondsF(const char* name,
                                        const base::TimeDelta& value) = 0;

   protected:
    virtual ~Enumerator() {}
  };

  // Outputs the fields in this structure to the provided enumerator.
  void EnumerateFields(Enumerator* enumerator) const;

  // Add fields of |other| to the fields in this structure.
  void Add(const RenderingStats& other);
};

}  // namespace cc

#endif  // CC_DEBUG_RENDERING_STATS_H_
