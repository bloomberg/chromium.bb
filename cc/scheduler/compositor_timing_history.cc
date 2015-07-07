// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/compositor_timing_history.h"

#include "base/metrics/histogram.h"
#include "base/trace_event/trace_event.h"
#include "cc/debug/rendering_stats_instrumentation.h"

// The estimates that affect the compositors deadline use the 100th percentile
// to avoid missing the Browser's deadline.
// The estimates related to main-thread responsiveness affect whether
// we attempt to recovery latency or not and use the 50th percentile.
// TODO(brianderson): Fine tune the percentiles below.
const size_t kDurationHistorySize = 60;
const double kBeginMainFrameToCommitEstimationPercentile = 50.0;
const double kCommitToReadyToActivateEstimationPercentile = 50.0;
const double kPrepareTilesEstimationPercentile = 100.0;
const double kActivateEstimationPercentile = 100.0;
const double kDrawEstimationPercentile = 100.0;

namespace cc {

CompositorTimingHistory::CompositorTimingHistory(
    RenderingStatsInstrumentation* rendering_stats_instrumentation)
    : enabled_(false),
      begin_main_frame_to_commit_duration_history_(kDurationHistorySize),
      commit_to_ready_to_activate_duration_history_(kDurationHistorySize),
      prepare_tiles_duration_history_(kDurationHistorySize),
      activate_duration_history_(kDurationHistorySize),
      draw_duration_history_(kDurationHistorySize),
      rendering_stats_instrumentation_(rendering_stats_instrumentation) {
}

CompositorTimingHistory::~CompositorTimingHistory() {
}

void CompositorTimingHistory::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetDouble("begin_main_frame_to_commit_duration_estimate_ms",
                   BeginMainFrameToCommitDurationEstimate().InMillisecondsF());
  state->SetDouble("commit_to_ready_to_activate_duration_estimate_ms",
                   CommitToReadyToActivateDurationEstimate().InMillisecondsF());
  state->SetDouble("prepare_tiles_duration_estimate_ms",
                   PrepareTilesDurationEstimate().InMillisecondsF());
  state->SetDouble("activate_duration_estimate_ms",
                   ActivateDurationEstimate().InMillisecondsF());
  state->SetDouble("draw_duration_estimate_ms",
                   DrawDurationEstimate().InMillisecondsF());
}

base::TimeTicks CompositorTimingHistory::Now() const {
  return base::TimeTicks::Now();
}

void CompositorTimingHistory::SetRecordingEnabled(bool enabled) {
  enabled_ = enabled;
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameToCommitDurationEstimate() const {
  return begin_main_frame_to_commit_duration_history_.Percentile(
      kBeginMainFrameToCommitEstimationPercentile);
}

base::TimeDelta
CompositorTimingHistory::CommitToReadyToActivateDurationEstimate() const {
  return commit_to_ready_to_activate_duration_history_.Percentile(
      kCommitToReadyToActivateEstimationPercentile);
}

base::TimeDelta CompositorTimingHistory::PrepareTilesDurationEstimate() const {
  return prepare_tiles_duration_history_.Percentile(
      kPrepareTilesEstimationPercentile);
}

base::TimeDelta CompositorTimingHistory::ActivateDurationEstimate() const {
  return activate_duration_history_.Percentile(kActivateEstimationPercentile);
}

base::TimeDelta CompositorTimingHistory::DrawDurationEstimate() const {
  return draw_duration_history_.Percentile(kDrawEstimationPercentile);
}

void CompositorTimingHistory::WillBeginMainFrame() {
  DCHECK_EQ(base::TimeTicks(), begin_main_frame_sent_time_);
  begin_main_frame_sent_time_ = Now();
}

void CompositorTimingHistory::BeginMainFrameAborted() {
  DidCommit();
}

void CompositorTimingHistory::DidCommit() {
  DCHECK_NE(base::TimeTicks(), begin_main_frame_sent_time_);

  commit_time_ = Now();

  base::TimeDelta begin_main_frame_to_commit_duration =
      commit_time_ - begin_main_frame_sent_time_;

  // Before adding the new data point to the timing history, see what we would
  // have predicted for this frame. This allows us to keep track of the accuracy
  // of our predictions.
  rendering_stats_instrumentation_->AddBeginMainFrameToCommitDuration(
      begin_main_frame_to_commit_duration,
      BeginMainFrameToCommitDurationEstimate());

  if (enabled_) {
    begin_main_frame_to_commit_duration_history_.InsertSample(
        begin_main_frame_to_commit_duration);
  }

  begin_main_frame_sent_time_ = base::TimeTicks();
}

void CompositorTimingHistory::WillPrepareTiles() {
  DCHECK_EQ(base::TimeTicks(), start_prepare_tiles_time_);
  start_prepare_tiles_time_ = Now();
}

void CompositorTimingHistory::DidPrepareTiles() {
  DCHECK_NE(base::TimeTicks(), start_prepare_tiles_time_);

  if (enabled_) {
    base::TimeDelta prepare_tiles_duration = Now() - start_prepare_tiles_time_;
    prepare_tiles_duration_history_.InsertSample(prepare_tiles_duration);
  }

  start_prepare_tiles_time_ = base::TimeTicks();
}

void CompositorTimingHistory::ReadyToActivate() {
  // We only care about the first ready to activate signal
  // after a commit.
  if (commit_time_ == base::TimeTicks())
    return;

  base::TimeDelta time_since_commit = Now() - commit_time_;

  // Before adding the new data point to the timing history, see what we would
  // have predicted for this frame. This allows us to keep track of the accuracy
  // of our predictions.
  rendering_stats_instrumentation_->AddCommitToActivateDuration(
      time_since_commit, CommitToReadyToActivateDurationEstimate());

  if (enabled_) {
    commit_to_ready_to_activate_duration_history_.InsertSample(
        time_since_commit);
  }

  commit_time_ = base::TimeTicks();
}

void CompositorTimingHistory::WillActivate() {
  DCHECK_EQ(base::TimeTicks(), start_activate_time_);
  start_activate_time_ = Now();
}

void CompositorTimingHistory::DidActivate() {
  DCHECK_NE(base::TimeTicks(), start_activate_time_);
  if (enabled_) {
    base::TimeDelta activate_duration = Now() - start_activate_time_;
    activate_duration_history_.InsertSample(activate_duration);
  }
  start_activate_time_ = base::TimeTicks();
}

void CompositorTimingHistory::WillDraw() {
  DCHECK_EQ(base::TimeTicks(), start_draw_time_);
  start_draw_time_ = Now();
}

void CompositorTimingHistory::DidDraw() {
  DCHECK_NE(base::TimeTicks(), start_draw_time_);
  base::TimeDelta draw_duration = Now() - start_draw_time_;

  // Before adding the new data point to the timing history, see what we would
  // have predicted for this frame. This allows us to keep track of the accuracy
  // of our predictions.
  base::TimeDelta draw_duration_estimate = DrawDurationEstimate();
  rendering_stats_instrumentation_->AddDrawDuration(draw_duration,
                                                    draw_duration_estimate);

  AddDrawDurationUMA(draw_duration, draw_duration_estimate);

  if (enabled_) {
    draw_duration_history_.InsertSample(draw_duration);
  }

  start_draw_time_ = base::TimeTicks();
}

void CompositorTimingHistory::AddDrawDurationUMA(
    base::TimeDelta draw_duration,
    base::TimeDelta draw_duration_estimate) {
  base::TimeDelta draw_duration_overestimate;
  base::TimeDelta draw_duration_underestimate;
  if (draw_duration > draw_duration_estimate)
    draw_duration_underestimate = draw_duration - draw_duration_estimate;
  else
    draw_duration_overestimate = draw_duration_estimate - draw_duration;
  UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.DrawDuration", draw_duration,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMilliseconds(100), 50);
  UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.DrawDurationUnderestimate",
                             draw_duration_underestimate,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMilliseconds(100), 50);
  UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.DrawDurationOverestimate",
                             draw_duration_overestimate,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMilliseconds(100), 50);
}

}  // namespace cc
