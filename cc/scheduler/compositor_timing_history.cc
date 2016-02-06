// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/compositor_timing_history.h"

#include <stddef.h>
#include <stdint.h>

#include "base/metrics/histogram.h"
#include "base/trace_event/trace_event.h"
#include "cc/debug/rendering_stats_instrumentation.h"

namespace cc {

class CompositorTimingHistory::UMAReporter {
 public:
  virtual ~UMAReporter() {}

  // Throughput measurements
  virtual void AddBeginMainFrameIntervalCritical(base::TimeDelta interval) = 0;
  virtual void AddBeginMainFrameIntervalNotCritical(
      base::TimeDelta interval) = 0;
  virtual void AddCommitInterval(base::TimeDelta interval) = 0;
  virtual void AddDrawInterval(base::TimeDelta interval) = 0;

  // Latency measurements
  virtual void AddBeginMainFrameToCommitDuration(base::TimeDelta duration,
                                                 base::TimeDelta estimate,
                                                 bool affects_estimate) = 0;
  virtual void AddBeginMainFrameQueueDurationCriticalDuration(
      base::TimeDelta duration,
      bool affects_estimate) = 0;
  virtual void AddBeginMainFrameQueueDurationNotCriticalDuration(
      base::TimeDelta duration,
      bool affects_estimate) = 0;
  virtual void AddBeginMainFrameStartToCommitDuration(
      base::TimeDelta duration,
      bool affects_estimate) = 0;
  virtual void AddCommitToReadyToActivateDuration(base::TimeDelta duration,
                                                  base::TimeDelta estimate,
                                                  bool affects_estimate) = 0;
  virtual void AddPrepareTilesDuration(base::TimeDelta duration,
                                       base::TimeDelta estimate,
                                       bool affects_estimate) = 0;
  virtual void AddActivateDuration(base::TimeDelta duration,
                                   base::TimeDelta estimate,
                                   bool affects_estimate) = 0;
  virtual void AddDrawDuration(base::TimeDelta duration,
                               base::TimeDelta estimate,
                               bool affects_estimate) = 0;
  virtual void AddSwapToAckLatency(base::TimeDelta duration) = 0;
};

namespace {

// Using the 90th percentile will disable latency recovery
// if we are missing the deadline approximately ~6 times per
// second.
// TODO(brianderson): Fine tune the percentiles below.
const size_t kDurationHistorySize = 60;
const double kBeginMainFrameToCommitEstimationPercentile = 90.0;
const double kBeginMainFrameQueueDurationEstimationPercentile = 90.0;
const double kBeginMainFrameQueueDurationCriticalEstimationPercentile = 90.0;
const double kBeginMainFrameQueueDurationNotCriticalEstimationPercentile = 90.0;
const double kBeginMainFrameStartToCommitEstimationPercentile = 90.0;
const double kCommitToReadyToActivateEstimationPercentile = 90.0;
const double kPrepareTilesEstimationPercentile = 90.0;
const double kActivateEstimationPercentile = 90.0;
const double kDrawEstimationPercentile = 90.0;

const int kUmaDurationMinMicros = 1;
const int64_t kUmaDurationMaxMicros = 1 * base::Time::kMicrosecondsPerSecond;
const size_t kUmaDurationBucketCount = 100;

// Deprecated because they combine Browser and Renderer stats and have low
// precision.
// TODO(brianderson): Remove.
void DeprecatedDrawDurationUMA(base::TimeDelta duration,
                               base::TimeDelta estimate) {
  base::TimeDelta duration_overestimate;
  base::TimeDelta duration_underestimate;
  if (duration > estimate)
    duration_underestimate = duration - estimate;
  else
    duration_overestimate = estimate - duration;
  UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.DrawDuration", duration,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMilliseconds(100), 50);
  UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.DrawDurationUnderestimate",
                             duration_underestimate,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMilliseconds(100), 50);
  UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.DrawDurationOverestimate",
                             duration_overestimate,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMilliseconds(100), 50);
}

#define UMA_HISTOGRAM_CUSTOM_TIMES_MICROS(name, sample)                     \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample.InMicroseconds(),                \
                              kUmaDurationMinMicros, kUmaDurationMaxMicros, \
                              kUmaDurationBucketCount);

// Records over/under estimates.
#define REPORT_COMPOSITOR_TIMING_HISTORY_UMA(category, subcategory)            \
  do {                                                                         \
    base::TimeDelta duration_overestimate;                                     \
    base::TimeDelta duration_underestimate;                                    \
    if (duration > estimate)                                                   \
      duration_underestimate = duration - estimate;                            \
    else                                                                       \
      duration_overestimate = estimate - duration;                             \
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS(                                         \
        "Scheduling." category "." subcategory "Duration", duration);          \
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS("Scheduling." category "." subcategory   \
                                      "Duration.Underestimate",                \
                                      duration_underestimate);                 \
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS("Scheduling." category "." subcategory   \
                                      "Duration.Overestimate",                 \
                                      duration_overestimate);                  \
    if (!affects_estimate) {                                                   \
      UMA_HISTOGRAM_CUSTOM_TIMES_MICROS("Scheduling." category "." subcategory \
                                        "Duration.NotUsedForEstimate",         \
                                        duration);                             \
    }                                                                          \
  } while (false)

// Does not record over/under estimates.
#define REPORT_COMPOSITOR_TIMING_HISTORY_UMA2(category, subcategory)           \
  do {                                                                         \
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS("Scheduling." category "." subcategory,  \
                                      duration);                               \
    if (!affects_estimate) {                                                   \
      UMA_HISTOGRAM_CUSTOM_TIMES_MICROS("Scheduling." category "." subcategory \
                                        ".NotUsedForEstimate",                 \
                                        duration);                             \
    }                                                                          \
  } while (false)

class RendererUMAReporter : public CompositorTimingHistory::UMAReporter {
 public:
  ~RendererUMAReporter() override {}

  void AddBeginMainFrameIntervalCritical(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS(
        "Scheduling.Renderer.BeginMainFrameIntervalCritical", interval);
  }

  void AddBeginMainFrameIntervalNotCritical(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS(
        "Scheduling.Renderer.BeginMainFrameIntervalNotCritical", interval);
  }

  void AddCommitInterval(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS("Scheduling.Renderer.CommitInterval",
                                      interval);
  }

  void AddDrawInterval(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS("Scheduling.Renderer.DrawInterval",
                                      interval);
  }

  void AddBeginMainFrameToCommitDuration(base::TimeDelta duration,
                                         base::TimeDelta estimate,
                                         bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA("Renderer", "BeginMainFrameToCommit");
  }

  void AddBeginMainFrameQueueDurationCriticalDuration(
      base::TimeDelta duration,
      bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA2(
        "Renderer", "BeginMainFrameQueueDurationCritical");
  }

  void AddBeginMainFrameQueueDurationNotCriticalDuration(
      base::TimeDelta duration,
      bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA2(
        "Renderer", "BeginMainFrameQueueDurationNotCritical");
  }

  void AddBeginMainFrameStartToCommitDuration(base::TimeDelta duration,
                                              bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA2(
        "Renderer", "BeginMainFrameStartToCommitDuration");
  }

  void AddCommitToReadyToActivateDuration(base::TimeDelta duration,
                                          base::TimeDelta estimate,
                                          bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA("Renderer", "CommitToReadyToActivate");
  }

  void AddPrepareTilesDuration(base::TimeDelta duration,
                               base::TimeDelta estimate,
                               bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA("Renderer", "PrepareTiles");
  }

  void AddActivateDuration(base::TimeDelta duration,
                           base::TimeDelta estimate,
                           bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA("Renderer", "Activate");
  }

  void AddDrawDuration(base::TimeDelta duration,
                       base::TimeDelta estimate,
                       bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA("Renderer", "Draw");
    DeprecatedDrawDurationUMA(duration, estimate);
  }

  void AddSwapToAckLatency(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS("Scheduling.Renderer.SwapToAckLatency",
                                      duration);
  }
};

class BrowserUMAReporter : public CompositorTimingHistory::UMAReporter {
 public:
  ~BrowserUMAReporter() override {}

  void AddBeginMainFrameIntervalCritical(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS(
        "Scheduling.Browser.BeginMainFrameIntervalCritical", interval);
  }

  void AddBeginMainFrameIntervalNotCritical(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS(
        "Scheduling.Browser.BeginMainFrameIntervalNotCritical", interval);
  }

  void AddCommitInterval(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS("Scheduling.Browser.CommitInterval",
                                      interval);
  }

  void AddDrawInterval(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS("Scheduling.Browser.DrawInterval",
                                      interval);
  }

  void AddBeginMainFrameToCommitDuration(base::TimeDelta duration,
                                         base::TimeDelta estimate,
                                         bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA("Browser", "BeginMainFrameToCommit");
  }

  void AddBeginMainFrameQueueDurationCriticalDuration(
      base::TimeDelta duration,
      bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA2(
        "Browser", "BeginMainFrameQueueDurationCritical");
  }

  void AddBeginMainFrameQueueDurationNotCriticalDuration(
      base::TimeDelta duration,
      bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA2(
        "Browser", "BeginMainFrameQueueDurationNotCritical");
  }

  void AddBeginMainFrameStartToCommitDuration(base::TimeDelta duration,
                                              bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA2("Browser",
                                          "BeginMainFrameStartToCommit");
  }

  void AddCommitToReadyToActivateDuration(base::TimeDelta duration,
                                          base::TimeDelta estimate,
                                          bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA("Browser", "CommitToReadyToActivate");
  }

  void AddPrepareTilesDuration(base::TimeDelta duration,
                               base::TimeDelta estimate,
                               bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA("Browser", "PrepareTiles");
  }

  void AddActivateDuration(base::TimeDelta duration,
                           base::TimeDelta estimate,
                           bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA("Browser", "Activate");
  }

  void AddDrawDuration(base::TimeDelta duration,
                       base::TimeDelta estimate,
                       bool affects_estimate) override {
    REPORT_COMPOSITOR_TIMING_HISTORY_UMA("Browser", "Draw");
    DeprecatedDrawDurationUMA(duration, estimate);
  }

  void AddSwapToAckLatency(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS("Scheduling.Browser.SwapToAckLatency",
                                      duration);
  }
};

class NullUMAReporter : public CompositorTimingHistory::UMAReporter {
 public:
  ~NullUMAReporter() override {}
  void AddBeginMainFrameIntervalCritical(base::TimeDelta interval) override {}
  void AddBeginMainFrameIntervalNotCritical(base::TimeDelta interval) override {
  }
  void AddCommitInterval(base::TimeDelta interval) override {}
  void AddDrawInterval(base::TimeDelta interval) override {}
  void AddBeginMainFrameToCommitDuration(base::TimeDelta duration,
                                         base::TimeDelta estimate,
                                         bool affects_estimate) override {}
  void AddBeginMainFrameQueueDurationCriticalDuration(
      base::TimeDelta duration,
      bool affects_estimate) override {}
  void AddBeginMainFrameQueueDurationNotCriticalDuration(
      base::TimeDelta duration,
      bool affects_estimate) override {}
  void AddBeginMainFrameStartToCommitDuration(base::TimeDelta duration,
                                              bool affects_estimate) override {}
  void AddCommitToReadyToActivateDuration(base::TimeDelta duration,
                                          base::TimeDelta estimate,
                                          bool affects_estimate) override {}
  void AddPrepareTilesDuration(base::TimeDelta duration,
                               base::TimeDelta estimate,
                               bool affects_estimate) override {}
  void AddActivateDuration(base::TimeDelta duration,
                           base::TimeDelta estimate,
                           bool affects_estimate) override {}
  void AddDrawDuration(base::TimeDelta duration,
                       base::TimeDelta estimate,
                       bool affects_estimate) override {}
  void AddSwapToAckLatency(base::TimeDelta duration) override {}
};

}  // namespace

CompositorTimingHistory::CompositorTimingHistory(
    UMACategory uma_category,
    RenderingStatsInstrumentation* rendering_stats_instrumentation)
    : enabled_(false),
      did_send_begin_main_frame_(false),
      begin_main_frame_needed_continuously_(false),
      begin_main_frame_committing_continuously_(false),
      compositor_drawing_continuously_(false),
      begin_main_frame_sent_to_commit_duration_history_(kDurationHistorySize),
      begin_main_frame_queue_duration_history_(kDurationHistorySize),
      begin_main_frame_queue_duration_critical_history_(kDurationHistorySize),
      begin_main_frame_queue_duration_not_critical_history_(
          kDurationHistorySize),
      begin_main_frame_start_to_commit_duration_history_(kDurationHistorySize),
      commit_to_ready_to_activate_duration_history_(kDurationHistorySize),
      prepare_tiles_duration_history_(kDurationHistorySize),
      activate_duration_history_(kDurationHistorySize),
      draw_duration_history_(kDurationHistorySize),
      begin_main_frame_on_critical_path_(false),
      uma_reporter_(CreateUMAReporter(uma_category)),
      rendering_stats_instrumentation_(rendering_stats_instrumentation) {}

CompositorTimingHistory::~CompositorTimingHistory() {
}

scoped_ptr<CompositorTimingHistory::UMAReporter>
CompositorTimingHistory::CreateUMAReporter(UMACategory category) {
  switch (category) {
    case RENDERER_UMA:
      return make_scoped_ptr(new RendererUMAReporter);
      break;
    case BROWSER_UMA:
      return make_scoped_ptr(new BrowserUMAReporter);
      break;
    case NULL_UMA:
      return make_scoped_ptr(new NullUMAReporter);
      break;
  }
  NOTREACHED();
  return make_scoped_ptr<CompositorTimingHistory::UMAReporter>(nullptr);
}

void CompositorTimingHistory::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetDouble("begin_main_frame_to_commit_estimate_ms",
                   BeginMainFrameToCommitDurationEstimate().InMillisecondsF());
  state->SetDouble("commit_to_ready_to_activate_estimate_ms",
                   CommitToReadyToActivateDurationEstimate().InMillisecondsF());
  state->SetDouble("prepare_tiles_estimate_ms",
                   PrepareTilesDurationEstimate().InMillisecondsF());
  state->SetDouble("activate_estimate_ms",
                   ActivateDurationEstimate().InMillisecondsF());
  state->SetDouble("draw_estimate_ms",
                   DrawDurationEstimate().InMillisecondsF());
}

base::TimeTicks CompositorTimingHistory::Now() const {
  return base::TimeTicks::Now();
}

void CompositorTimingHistory::SetRecordingEnabled(bool enabled) {
  enabled_ = enabled;
}

void CompositorTimingHistory::SetBeginMainFrameNeededContinuously(bool active) {
  if (active == begin_main_frame_needed_continuously_)
    return;
  begin_main_frame_end_time_prev_ = base::TimeTicks();
  begin_main_frame_needed_continuously_ = active;
}

void CompositorTimingHistory::SetBeginMainFrameCommittingContinuously(
    bool active) {
  if (active == begin_main_frame_committing_continuously_)
    return;
  new_active_tree_draw_end_time_prev_ = base::TimeTicks();
  begin_main_frame_committing_continuously_ = active;
}

void CompositorTimingHistory::SetCompositorDrawingContinuously(bool active) {
  if (active == compositor_drawing_continuously_)
    return;
  draw_end_time_prev_ = base::TimeTicks();
  compositor_drawing_continuously_ = active;
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameToCommitDurationEstimate() const {
  return begin_main_frame_sent_to_commit_duration_history_.Percentile(
      kBeginMainFrameToCommitEstimationPercentile);
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameQueueDurationCriticalEstimate() const {
  base::TimeDelta all = begin_main_frame_queue_duration_history_.Percentile(
      kBeginMainFrameQueueDurationEstimationPercentile);
  base::TimeDelta critical =
      begin_main_frame_queue_duration_critical_history_.Percentile(
          kBeginMainFrameQueueDurationCriticalEstimationPercentile);
  // Return the min since critical BeginMainFrames are likely fast if
  // the non critical ones are.
  return std::min(critical, all);
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameQueueDurationNotCriticalEstimate()
    const {
  base::TimeDelta all = begin_main_frame_queue_duration_history_.Percentile(
      kBeginMainFrameQueueDurationEstimationPercentile);
  base::TimeDelta not_critical =
      begin_main_frame_queue_duration_not_critical_history_.Percentile(
          kBeginMainFrameQueueDurationNotCriticalEstimationPercentile);
  // Return the max since, non critical BeginMainFrames are likely slow if
  // the critical ones are.
  return std::max(not_critical, all);
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameStartToCommitDurationEstimate() const {
  return begin_main_frame_start_to_commit_duration_history_.Percentile(
      kBeginMainFrameStartToCommitEstimationPercentile);
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

void CompositorTimingHistory::WillBeginImplFrame(
    bool new_active_tree_is_likely) {
  // The check for whether a BeginMainFrame was sent anytime between two
  // BeginImplFrames protects us from not detecting a fast main thread that
  // does all it's work and goes idle in between BeginImplFrames.
  // For example, this may happen if an animation is being driven with
  // setInterval(17) or if input events just happen to arrive in the
  // middle of every frame.
  if (!new_active_tree_is_likely && !did_send_begin_main_frame_) {
    SetBeginMainFrameNeededContinuously(false);
    SetBeginMainFrameCommittingContinuously(false);
  }

  did_send_begin_main_frame_ = false;
}

void CompositorTimingHistory::WillFinishImplFrame(bool needs_redraw) {
  if (!needs_redraw)
    SetCompositorDrawingContinuously(false);
}

void CompositorTimingHistory::BeginImplFrameNotExpectedSoon() {
  SetBeginMainFrameNeededContinuously(false);
  SetBeginMainFrameCommittingContinuously(false);
  SetCompositorDrawingContinuously(false);
}

void CompositorTimingHistory::WillBeginMainFrame(bool on_critical_path) {
  DCHECK_EQ(base::TimeTicks(), begin_main_frame_sent_time_);
  begin_main_frame_on_critical_path_ = on_critical_path;
  begin_main_frame_sent_time_ = Now();

  did_send_begin_main_frame_ = true;
  SetBeginMainFrameNeededContinuously(true);
}

void CompositorTimingHistory::BeginMainFrameStarted(
    base::TimeTicks main_thread_start_time) {
  DCHECK_NE(base::TimeTicks(), begin_main_frame_sent_time_);
  DCHECK_EQ(base::TimeTicks(), begin_main_frame_start_time_);
  begin_main_frame_start_time_ = main_thread_start_time;
}

void CompositorTimingHistory::BeginMainFrameAborted() {
  SetBeginMainFrameCommittingContinuously(false);
  DidBeginMainFrame();
}

void CompositorTimingHistory::DidCommit() {
  SetBeginMainFrameCommittingContinuously(true);
  DidBeginMainFrame();
}

void CompositorTimingHistory::DidBeginMainFrame() {
  DCHECK_NE(base::TimeTicks(), begin_main_frame_sent_time_);

  begin_main_frame_end_time_ = Now();

  // If the BeginMainFrame start time isn't know, assume it was immediate
  // for scheduling purposes, but don't report it for UMA to avoid skewing
  // the results.
  bool begin_main_frame_start_time_is_valid =
      !begin_main_frame_start_time_.is_null();
  if (!begin_main_frame_start_time_is_valid)
    begin_main_frame_start_time_ = begin_main_frame_sent_time_;

  base::TimeDelta begin_main_frame_sent_to_commit_duration =
      begin_main_frame_end_time_ - begin_main_frame_sent_time_;
  base::TimeDelta begin_main_frame_queue_duration =
      begin_main_frame_start_time_ - begin_main_frame_sent_time_;
  base::TimeDelta begin_main_frame_start_to_commit_duration =
      begin_main_frame_end_time_ - begin_main_frame_start_time_;

  // Before adding the new data point to the timing history, see what we would
  // have predicted for this frame. This allows us to keep track of the accuracy
  // of our predictions.
  base::TimeDelta begin_main_frame_sent_to_commit_estimate =
      BeginMainFrameToCommitDurationEstimate();
  uma_reporter_->AddBeginMainFrameToCommitDuration(
      begin_main_frame_sent_to_commit_duration,
      begin_main_frame_sent_to_commit_estimate, enabled_);
  rendering_stats_instrumentation_->AddBeginMainFrameToCommitDuration(
      begin_main_frame_sent_to_commit_duration,
      begin_main_frame_sent_to_commit_estimate);

  if (begin_main_frame_start_time_is_valid) {
    if (begin_main_frame_on_critical_path_) {
      uma_reporter_->AddBeginMainFrameQueueDurationCriticalDuration(
          begin_main_frame_queue_duration, enabled_);
    } else {
      uma_reporter_->AddBeginMainFrameQueueDurationNotCriticalDuration(
          begin_main_frame_queue_duration, enabled_);
    }
  }

  uma_reporter_->AddBeginMainFrameStartToCommitDuration(
      begin_main_frame_start_to_commit_duration, enabled_);

  if (enabled_) {
    begin_main_frame_sent_to_commit_duration_history_.InsertSample(
        begin_main_frame_sent_to_commit_duration);
    begin_main_frame_queue_duration_history_.InsertSample(
        begin_main_frame_queue_duration);
    if (begin_main_frame_on_critical_path_) {
      begin_main_frame_queue_duration_critical_history_.InsertSample(
          begin_main_frame_queue_duration);
    } else {
      begin_main_frame_queue_duration_not_critical_history_.InsertSample(
          begin_main_frame_queue_duration);
    }
    begin_main_frame_start_to_commit_duration_history_.InsertSample(
        begin_main_frame_start_to_commit_duration);
  }

  if (begin_main_frame_needed_continuously_) {
    if (!begin_main_frame_end_time_prev_.is_null()) {
      base::TimeDelta commit_interval =
          begin_main_frame_end_time_ - begin_main_frame_end_time_prev_;
      if (begin_main_frame_on_critical_path_)
        uma_reporter_->AddBeginMainFrameIntervalCritical(commit_interval);
      else
        uma_reporter_->AddBeginMainFrameIntervalNotCritical(commit_interval);
    }
    begin_main_frame_end_time_prev_ = begin_main_frame_end_time_;
  }

  begin_main_frame_sent_time_ = base::TimeTicks();
  begin_main_frame_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::WillPrepareTiles() {
  DCHECK_EQ(base::TimeTicks(), prepare_tiles_start_time_);
  prepare_tiles_start_time_ = Now();
}

void CompositorTimingHistory::DidPrepareTiles() {
  DCHECK_NE(base::TimeTicks(), prepare_tiles_start_time_);

  base::TimeDelta prepare_tiles_duration = Now() - prepare_tiles_start_time_;
  uma_reporter_->AddPrepareTilesDuration(
      prepare_tiles_duration, PrepareTilesDurationEstimate(), enabled_);
  if (enabled_)
    prepare_tiles_duration_history_.InsertSample(prepare_tiles_duration);

  prepare_tiles_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::ReadyToActivate() {
  // We only care about the first ready to activate signal
  // after a commit.
  if (begin_main_frame_end_time_ == base::TimeTicks())
    return;

  base::TimeDelta time_since_commit = Now() - begin_main_frame_end_time_;

  // Before adding the new data point to the timing history, see what we would
  // have predicted for this frame. This allows us to keep track of the accuracy
  // of our predictions.

  base::TimeDelta commit_to_ready_to_activate_estimate =
      CommitToReadyToActivateDurationEstimate();
  uma_reporter_->AddCommitToReadyToActivateDuration(
      time_since_commit, commit_to_ready_to_activate_estimate, enabled_);
  rendering_stats_instrumentation_->AddCommitToActivateDuration(
      time_since_commit, commit_to_ready_to_activate_estimate);

  if (enabled_) {
    commit_to_ready_to_activate_duration_history_.InsertSample(
        time_since_commit);
  }

  begin_main_frame_end_time_ = base::TimeTicks();
}

void CompositorTimingHistory::WillActivate() {
  DCHECK_EQ(base::TimeTicks(), activate_start_time_);
  activate_start_time_ = Now();
}

void CompositorTimingHistory::DidActivate() {
  DCHECK_NE(base::TimeTicks(), activate_start_time_);
  base::TimeDelta activate_duration = Now() - activate_start_time_;

  uma_reporter_->AddActivateDuration(activate_duration,
                                     ActivateDurationEstimate(), enabled_);
  if (enabled_)
    activate_duration_history_.InsertSample(activate_duration);

  activate_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::WillDraw() {
  DCHECK_EQ(base::TimeTicks(), draw_start_time_);
  draw_start_time_ = Now();
}

void CompositorTimingHistory::DidDraw(bool used_new_active_tree) {
  DCHECK_NE(base::TimeTicks(), draw_start_time_);
  base::TimeTicks draw_end_time = Now();
  base::TimeDelta draw_duration = draw_end_time - draw_start_time_;

  // Before adding the new data point to the timing history, see what we would
  // have predicted for this frame. This allows us to keep track of the accuracy
  // of our predictions.
  base::TimeDelta draw_estimate = DrawDurationEstimate();
  rendering_stats_instrumentation_->AddDrawDuration(draw_duration,
                                                    draw_estimate);

  uma_reporter_->AddDrawDuration(draw_duration, draw_estimate, enabled_);

  if (enabled_) {
    draw_duration_history_.InsertSample(draw_duration);
  }

  SetCompositorDrawingContinuously(true);
  if (!draw_end_time_prev_.is_null()) {
    base::TimeDelta draw_interval = draw_end_time - draw_end_time_prev_;
    uma_reporter_->AddDrawInterval(draw_interval);
  }
  draw_end_time_prev_ = draw_end_time;

  if (begin_main_frame_committing_continuously_ && used_new_active_tree) {
    if (!new_active_tree_draw_end_time_prev_.is_null()) {
      base::TimeDelta draw_interval =
          draw_end_time - new_active_tree_draw_end_time_prev_;
      uma_reporter_->AddCommitInterval(draw_interval);
    }
    new_active_tree_draw_end_time_prev_ = draw_end_time;
  }

  draw_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::DidSwapBuffers() {
  DCHECK_EQ(base::TimeTicks(), swap_start_time_);
  swap_start_time_ = Now();
}

void CompositorTimingHistory::DidSwapBuffersComplete() {
  DCHECK_NE(base::TimeTicks(), swap_start_time_);
  base::TimeDelta swap_to_ack_duration = Now() - swap_start_time_;
  uma_reporter_->AddSwapToAckLatency(swap_to_ack_duration);
  swap_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::DidSwapBuffersReset() {
  swap_start_time_ = base::TimeTicks();
}

}  // namespace cc
