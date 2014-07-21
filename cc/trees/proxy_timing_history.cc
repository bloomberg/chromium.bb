// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/proxy_timing_history.h"

#include "base/metrics/histogram.h"

const size_t kDurationHistorySize = 60;
const double kCommitAndActivationDurationEstimationPercentile = 50.0;
const double kDrawDurationEstimationPercentile = 100.0;
const int kDrawDurationEstimatePaddingInMicroseconds = 0;

namespace cc {

ProxyTimingHistory::ProxyTimingHistory(
    RenderingStatsInstrumentation* rendering_stats_instrumentation)
    : draw_duration_history_(kDurationHistorySize),
      begin_main_frame_to_commit_duration_history_(kDurationHistorySize),
      commit_to_activate_duration_history_(kDurationHistorySize),
      rendering_stats_instrumentation_(rendering_stats_instrumentation) {
}

ProxyTimingHistory::~ProxyTimingHistory() {}

base::TimeDelta ProxyTimingHistory::DrawDurationEstimate() const {
  base::TimeDelta historical_estimate =
      draw_duration_history_.Percentile(kDrawDurationEstimationPercentile);
  base::TimeDelta padding = base::TimeDelta::FromMicroseconds(
      kDrawDurationEstimatePaddingInMicroseconds);
  return historical_estimate + padding;
}

base::TimeDelta ProxyTimingHistory::BeginMainFrameToCommitDurationEstimate()
    const {
  return begin_main_frame_to_commit_duration_history_.Percentile(
      kCommitAndActivationDurationEstimationPercentile);
}

base::TimeDelta ProxyTimingHistory::CommitToActivateDurationEstimate() const {
  return commit_to_activate_duration_history_.Percentile(
      kCommitAndActivationDurationEstimationPercentile);
}

void ProxyTimingHistory::DidBeginMainFrame() {
  begin_main_frame_sent_time_ = base::TimeTicks::HighResNow();
}

void ProxyTimingHistory::DidCommit() {
  commit_complete_time_ = base::TimeTicks::HighResNow();
  base::TimeDelta begin_main_frame_to_commit_duration =
      commit_complete_time_ - begin_main_frame_sent_time_;

  // Before adding the new data point to the timing history, see what we would
  // have predicted for this frame. This allows us to keep track of the accuracy
  // of our predictions.
  rendering_stats_instrumentation_->AddBeginMainFrameToCommitDuration(
      begin_main_frame_to_commit_duration,
      BeginMainFrameToCommitDurationEstimate());

  begin_main_frame_to_commit_duration_history_.InsertSample(
      begin_main_frame_to_commit_duration);
}

void ProxyTimingHistory::DidActivateSyncTree() {
  base::TimeDelta commit_to_activate_duration =
      base::TimeTicks::HighResNow() - commit_complete_time_;

  // Before adding the new data point to the timing history, see what we would
  // have predicted for this frame. This allows us to keep track of the accuracy
  // of our predictions.
  rendering_stats_instrumentation_->AddCommitToActivateDuration(
      commit_to_activate_duration, CommitToActivateDurationEstimate());

  commit_to_activate_duration_history_.InsertSample(
      commit_to_activate_duration);
}

void ProxyTimingHistory::DidStartDrawing() {
  start_draw_time_ = base::TimeTicks::HighResNow();
}

void ProxyTimingHistory::DidFinishDrawing() {
  base::TimeDelta draw_duration =
      base::TimeTicks::HighResNow() - start_draw_time_;

  // Before adding the new data point to the timing history, see what we would
  // have predicted for this frame. This allows us to keep track of the accuracy
  // of our predictions.
  base::TimeDelta draw_duration_estimate = DrawDurationEstimate();
  rendering_stats_instrumentation_->AddDrawDuration(draw_duration,
                                                    draw_duration_estimate);

  AddDrawDurationUMA(draw_duration, draw_duration_estimate);

  draw_duration_history_.InsertSample(draw_duration);
}

void ProxyTimingHistory::AddDrawDurationUMA(
    base::TimeDelta draw_duration,
    base::TimeDelta draw_duration_estimate) {
  base::TimeDelta draw_duration_overestimate;
  base::TimeDelta draw_duration_underestimate;
  if (draw_duration > draw_duration_estimate)
    draw_duration_underestimate = draw_duration - draw_duration_estimate;
  else
    draw_duration_overestimate = draw_duration_estimate - draw_duration;
  UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.DrawDuration",
                             draw_duration,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMilliseconds(100),
                             50);
  UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.DrawDurationUnderestimate",
                             draw_duration_underestimate,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMilliseconds(100),
                             50);
  UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.DrawDurationOverestimate",
                             draw_duration_overestimate,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMilliseconds(100),
                             50);
}

}  // namespace cc
