// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_COMPOSITOR_TIMING_HISTORY_H_
#define CC_SCHEDULER_COMPOSITOR_TIMING_HISTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/rolling_time_delta_history.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

class RenderingStatsInstrumentation;

class CC_EXPORT CompositorTimingHistory {
 public:
  enum UMACategory {
    RENDERER_UMA,
    BROWSER_UMA,
    NULL_UMA,
  };
  class UMAReporter;

  CompositorTimingHistory(
      UMACategory uma_category,
      RenderingStatsInstrumentation* rendering_stats_instrumentation);
  virtual ~CompositorTimingHistory();

  void AsValueInto(base::trace_event::TracedValue* state) const;

  // Deprecated: http://crbug.com/552004
  virtual base::TimeDelta BeginMainFrameToCommitDurationEstimate() const;

  // The main thread responsiveness depends heavily on whether or not the
  // on_critical_path flag is set, so we record response times separately.
  virtual base::TimeDelta BeginMainFrameQueueDurationCriticalEstimate() const;
  virtual base::TimeDelta BeginMainFrameQueueDurationNotCriticalEstimate()
      const;
  virtual base::TimeDelta BeginMainFrameStartToCommitDurationEstimate() const;
  virtual base::TimeDelta CommitToReadyToActivateDurationEstimate() const;
  virtual base::TimeDelta PrepareTilesDurationEstimate() const;
  virtual base::TimeDelta ActivateDurationEstimate() const;
  virtual base::TimeDelta DrawDurationEstimate() const;

  void SetRecordingEnabled(bool enabled);

  void WillBeginImplFrame(bool new_active_tree_is_likely);
  void WillFinishImplFrame(bool needs_redraw);
  void BeginImplFrameNotExpectedSoon();
  void WillBeginMainFrame(bool on_critical_path);
  void BeginMainFrameStarted(base::TimeTicks main_thread_start_time);
  void BeginMainFrameAborted();
  void DidCommit();
  void WillPrepareTiles();
  void DidPrepareTiles();
  void ReadyToActivate();
  void WillActivate();
  void DidActivate();
  void WillDraw();
  void DidDraw(bool used_new_active_tree);
  void DidSwapBuffers();
  void DidSwapBuffersComplete();
  void DidSwapBuffersReset();

 protected:
  void DidBeginMainFrame();

  void SetBeginMainFrameNeededContinuously(bool active);
  void SetBeginMainFrameCommittingContinuously(bool active);
  void SetCompositorDrawingContinuously(bool active);

  static scoped_ptr<UMAReporter> CreateUMAReporter(UMACategory category);
  virtual base::TimeTicks Now() const;

  bool enabled_;

  // Used to calculate frame rates of Main and Impl threads.
  bool did_send_begin_main_frame_;
  bool begin_main_frame_needed_continuously_;
  bool begin_main_frame_committing_continuously_;
  bool compositor_drawing_continuously_;
  base::TimeTicks begin_main_frame_end_time_prev_;
  base::TimeTicks new_active_tree_draw_end_time_prev_;
  base::TimeTicks draw_end_time_prev_;

  RollingTimeDeltaHistory begin_main_frame_sent_to_commit_duration_history_;
  RollingTimeDeltaHistory begin_main_frame_queue_duration_history_;
  RollingTimeDeltaHistory begin_main_frame_queue_duration_critical_history_;
  RollingTimeDeltaHistory begin_main_frame_queue_duration_not_critical_history_;
  RollingTimeDeltaHistory begin_main_frame_start_to_commit_duration_history_;
  RollingTimeDeltaHistory commit_to_ready_to_activate_duration_history_;
  RollingTimeDeltaHistory prepare_tiles_duration_history_;
  RollingTimeDeltaHistory activate_duration_history_;
  RollingTimeDeltaHistory draw_duration_history_;

  bool begin_main_frame_on_critical_path_;
  base::TimeTicks begin_main_frame_sent_time_;
  base::TimeTicks begin_main_frame_start_time_;
  base::TimeTicks begin_main_frame_end_time_;
  base::TimeTicks prepare_tiles_start_time_;
  base::TimeTicks activate_start_time_;
  base::TimeTicks draw_start_time_;
  base::TimeTicks swap_start_time_;

  scoped_ptr<UMAReporter> uma_reporter_;
  RenderingStatsInstrumentation* rendering_stats_instrumentation_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositorTimingHistory);
};

}  // namespace cc

#endif  // CC_SCHEDULER_COMPOSITOR_TIMING_HISTORY_H_
