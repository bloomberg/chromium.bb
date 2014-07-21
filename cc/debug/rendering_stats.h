// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_RENDERING_STATS_H_
#define CC_DEBUG_RENDERING_STATS_H_

#include <list>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/base/cc_export.h"
#include "cc/debug/traced_value.h"

namespace cc {

struct CC_EXPORT RenderingStats {
  // Stores a sequence of TimeDelta objects.
  class CC_EXPORT TimeDeltaList {
   public:
    TimeDeltaList();
    ~TimeDeltaList();

    void Append(base::TimeDelta value);
    scoped_ptr<base::ListValue> AsListValueInMilliseconds() const;
    void Add(const TimeDeltaList& other);

   private:
    std::list<base::TimeDelta> values;
  };

  struct CC_EXPORT MainThreadRenderingStats {
    // Note: when adding new members, please remember to update Add in
    // rendering_stats.cc.

    int64 frame_count;
    base::TimeDelta paint_time;
    int64 painted_pixel_count;
    base::TimeDelta record_time;
    int64 recorded_pixel_count;

    MainThreadRenderingStats();
    ~MainThreadRenderingStats();
    scoped_refptr<base::debug::ConvertableToTraceFormat> AsTraceableData()
        const;
    void Add(const MainThreadRenderingStats& other);
  };

  struct CC_EXPORT ImplThreadRenderingStats {
    // Note: when adding new members, please remember to update Add in
    // rendering_stats.cc.

    int64 frame_count;
    base::TimeDelta rasterize_time;
    base::TimeDelta analysis_time;
    int64 rasterized_pixel_count;
    int64 visible_content_area;
    int64 approximated_visible_content_area;

    TimeDeltaList draw_duration;
    TimeDeltaList draw_duration_estimate;
    TimeDeltaList begin_main_frame_to_commit_duration;
    TimeDeltaList begin_main_frame_to_commit_duration_estimate;
    TimeDeltaList commit_to_activate_duration;
    TimeDeltaList commit_to_activate_duration_estimate;

    ImplThreadRenderingStats();
    ~ImplThreadRenderingStats();
    scoped_refptr<base::debug::ConvertableToTraceFormat> AsTraceableData()
        const;
    void Add(const ImplThreadRenderingStats& other);
  };

  MainThreadRenderingStats main_stats;
  ImplThreadRenderingStats impl_stats;

  // Add fields of |other| to the fields in this structure.
  void Add(const RenderingStats& other);
};

}  // namespace cc

#endif  // CC_DEBUG_RENDERING_STATS_H_
