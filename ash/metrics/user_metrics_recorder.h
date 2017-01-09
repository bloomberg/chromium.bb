// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_USER_METRICS_RECORDER_H_
#define ASH_METRICS_USER_METRICS_RECORDER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/common/metrics/user_metrics_action.h"
#include "ash/metrics/task_switch_metrics_recorder.h"
#include "base/macros.h"
#include "base/timer/timer.h"

namespace ash {

class DesktopTaskSwitchMetricRecorder;
class PointerMetricsRecorder;

namespace test {
class UserMetricsRecorderTestAPI;
}

// User Metrics Recorder provides a repeating callback (RecordPeriodicMetrics)
// on a timer to allow recording of state data over time to the UMA records.
// Any additional states (in ash) that require monitoring can be added to
// this class. As well calls to record on action metrics
// (RecordUserMetricsAction) are passed through the UserMetricsRecorder.
class ASH_EXPORT UserMetricsRecorder {
 public:
  // Creates a UserMetricsRecorder that records metrics periodically. Equivalent
  // to calling UserMetricsRecorder(true).
  UserMetricsRecorder();

  virtual ~UserMetricsRecorder();

  // Records an Ash owned user action.
  void RecordUserMetricsAction(UserMetricsAction action);

  TaskSwitchMetricsRecorder& task_switch_metrics_recorder() {
    return task_switch_metrics_recorder_;
  }

  // Informs |this| that the Shell has been initialized.
  void OnShellInitialized();

  // Informs |this| that the Shell is going to be shut down.
  void OnShellShuttingDown();

 private:
  friend class test::UserMetricsRecorderTestAPI;

  // Creates a UserMetricsRecorder and will only record periodic metrics if
  // |record_periodic_metrics| is true. This is used by tests that do not want
  // the timer to be started.
  // TODO(bruthig): Add a constructor that accepts a base::RepeatingTimer so
  // that tests can inject a test double that can be controlled by the test. The
  // missing piece is a suitable base::RepeatingTimer test double.
  explicit UserMetricsRecorder(bool record_periodic_metrics);

  // Records UMA metrics. Invoked periodically by the |timer_|.
  void RecordPeriodicMetrics();

  // Returns true if the user's session is active and they are in a desktop
  // environment.
  bool IsUserInActiveDesktopEnvironment() const;

  // Starts the |timer_| and binds it to |RecordPeriodicMetrics|.
  void StartTimer();

  // The periodic timer that triggers metrics to be recorded.
  base::RepeatingTimer timer_;

  TaskSwitchMetricsRecorder task_switch_metrics_recorder_;

  // Metric recorder to track how often task windows are activated by mouse
  // clicks or touchscreen taps.
  std::unique_ptr<DesktopTaskSwitchMetricRecorder>
      desktop_task_switch_metric_recorder_;

  // Metric recorder to track pointer down events.
  std::unique_ptr<PointerMetricsRecorder> pointer_metrics_recorder_;

  DISALLOW_COPY_AND_ASSIGN(UserMetricsRecorder);
};

}  // namespace ash

#endif  // ASH_METRICS_USER_METRICS_RECORDER_H_
