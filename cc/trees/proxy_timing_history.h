// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROXY_TIMING_HISTORY_H_
#define CC_TREES_PROXY_TIMING_HISTORY_H_

#include "cc/base/rolling_time_delta_history.h"
#include "cc/debug/rendering_stats_instrumentation.h"

namespace cc {

class ProxyTimingHistory {
 public:
  explicit ProxyTimingHistory(
      RenderingStatsInstrumentation* rendering_stats_instrumentation);
  ~ProxyTimingHistory();

  base::TimeDelta DrawDurationEstimate() const;
  base::TimeDelta BeginMainFrameToCommitDurationEstimate() const;
  base::TimeDelta CommitToActivateDurationEstimate() const;

  void DidBeginMainFrame();
  void DidCommit();
  void DidActivateSyncTree();
  void DidStartDrawing();
  void DidFinishDrawing();

 protected:
  void AddDrawDurationUMA(base::TimeDelta draw_duration,
                          base::TimeDelta draw_duration_estimate);

  RollingTimeDeltaHistory draw_duration_history_;
  RollingTimeDeltaHistory begin_main_frame_to_commit_duration_history_;
  RollingTimeDeltaHistory commit_to_activate_duration_history_;

  base::TimeTicks begin_main_frame_sent_time_;
  base::TimeTicks commit_complete_time_;
  base::TimeTicks start_draw_time_;

  RenderingStatsInstrumentation* rendering_stats_instrumentation_;
};

}  // namespace cc

#endif  // CC_TREES_PROXY_TIMING_HISTORY_H_
