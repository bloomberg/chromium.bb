// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/task_switch_metrics_recorder.h"

#include "ash/metrics/task_switch_time_tracker.h"

namespace ash {

namespace {

const char kAshTaskSwitchHistogramName[] = "Ash.TimeBetweenTaskSwitches";

const char kDesktopHistogramName[] =
    "Ash.Desktop.TimeBetweenNavigateToTaskSwitches";

const char kShelfHistogramName[] =
    "Ash.Shelf.TimeBetweenNavigateToTaskSwitches";

const char kTabStripHistogramName[] =
    "Ash.Tab.TimeBetweenSwitchToExistingTabUserActions";

const char kAcceleratorWindowCycleHistogramName[] =
    "Ash.WindowCycleController.TimeBetweenTaskSwitches";

const char kAppListHistogramName[] = "Ash.AppList.TimeBetweenTaskSwitches";

const char kOverviewModeHistogramName[] =
    "Ash.WindowSelector.TimeBetweenActiveWindowChanges";

// Returns the histogram name for the given |task_switch_source|.
const char* GetHistogramName(
    TaskSwitchMetricsRecorder::TaskSwitchSource task_switch_source) {
  switch (task_switch_source) {
    case TaskSwitchMetricsRecorder::ANY:
      return kAshTaskSwitchHistogramName;
    case TaskSwitchMetricsRecorder::APP_LIST:
      return kAppListHistogramName;
    case TaskSwitchMetricsRecorder::DESKTOP:
      return kDesktopHistogramName;
    case TaskSwitchMetricsRecorder::OVERVIEW_MODE:
      return kOverviewModeHistogramName;
    case TaskSwitchMetricsRecorder::SHELF:
      return kShelfHistogramName;
    case TaskSwitchMetricsRecorder::TAB_STRIP:
      return kTabStripHistogramName;
    case TaskSwitchMetricsRecorder::WINDOW_CYCLE_CONTROLLER:
      return kAcceleratorWindowCycleHistogramName;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

TaskSwitchMetricsRecorder::TaskSwitchMetricsRecorder() {
}

TaskSwitchMetricsRecorder::~TaskSwitchMetricsRecorder() {
}

void TaskSwitchMetricsRecorder::OnTaskSwitch(
    TaskSwitchSource task_switch_source) {
  DCHECK_NE(task_switch_source, ANY);
  if (task_switch_source != ANY) {
    OnTaskSwitchInternal(task_switch_source);
    OnTaskSwitchInternal(ANY);
  }
}

void TaskSwitchMetricsRecorder::OnTaskSwitchInternal(
    TaskSwitchSource task_switch_source) {
  TaskSwitchTimeTracker* task_switch_time_tracker =
      FindTaskSwitchTimeTracker(task_switch_source);
  if (!task_switch_time_tracker)
    AddTaskSwitchTimeTracker(task_switch_source);

  task_switch_time_tracker = FindTaskSwitchTimeTracker(task_switch_source);
  CHECK(task_switch_time_tracker);

  task_switch_time_tracker->OnTaskSwitch();
}

TaskSwitchTimeTracker* TaskSwitchMetricsRecorder::FindTaskSwitchTimeTracker(
    TaskSwitchSource task_switch_source) {
  return histogram_map_.get(task_switch_source);
}

void TaskSwitchMetricsRecorder::AddTaskSwitchTimeTracker(
    TaskSwitchSource task_switch_source) {
  CHECK(histogram_map_.find(task_switch_source) == histogram_map_.end());

  const char* histogram_name = GetHistogramName(task_switch_source);
  DCHECK(histogram_name);

  histogram_map_.add(
      task_switch_source,
      make_scoped_ptr(new TaskSwitchTimeTracker(histogram_name)));
}

}  // namespace ash
