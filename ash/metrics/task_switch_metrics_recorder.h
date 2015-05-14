// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_TASK_SWITCH_METRIC_RECORDER_H_
#define ASH_METRICS_TASK_SWITCH_METRIC_RECORDER_H_

#include <string>

#include "ash/ash_export.h"
#include "base/containers/scoped_ptr_hash_map.h"

namespace ash {

class TaskSwitchTimeTracker;

// The TaskSwitchMetricsRecorder class records UMA metrics related to task
// switching. The main purpose of the TaskSwitchMetricsRecorder is to track time
// deltas between task switches and record histograms of the deltas.
class ASH_EXPORT TaskSwitchMetricsRecorder {
 public:
  // Enumeration of the different user interfaces that could be the source of
  // a task switch. Note this is not necessarily comprehensive of all sources.
  enum TaskSwitchSource {
    // All task switches caused by shelf buttons, not including sub-menus.
    kShelf
  };

  TaskSwitchMetricsRecorder();
  virtual ~TaskSwitchMetricsRecorder();

  // Notifies |this| that a "navigate to" task switch has occurred. A
  // "navigate to" operation is defined by a task switch where the specific task
  // that becomes active is user-predictable (ie Alt+Tab accelerator, launching
  // a new window via the shelf, etc). Contrast to a "navigate away" operation
  // which is defined as a user interaction that navigates away from a specified
  // task and the next task that becomes active is likely not user-predictable
  // (ie. closing or minimizing a window, closing a tab, etc).
  //
  // Will add an entry to |histogram_map_| when called for the first time for
  // each |task_switch_source| value.
  void OnTaskSwitch(TaskSwitchSource task_switch_source);

 private:
  // Returns the TaskSwitchTimeTracker associated with the specified
  // |task_switch_source|. May return nullptr if mapping does not exist yet.
  TaskSwitchTimeTracker* FindTaskSwitchTimeTracker(
      TaskSwitchSource task_switch_source);

  // Adds a TaskSwitchTimeTracker to |histogram_map_| for the specified
  // |task_switch_source|. Behavior is undefined if a TaskSwitchTimeTracker
  // |histogram_map_| already has an entry for |task_switch_source|.
  void AddTaskSwitchTimeTracker(TaskSwitchSource task_switch_source);

  // Tracks TaskSwitchSource to TaskSwitchTimeTracker mappings. The
  // |histogram_map_| is populated on demand the first time a
  // TaskSwitchTimeTracker is needed for a given source.
  base::ScopedPtrHashMap<int, scoped_ptr<TaskSwitchTimeTracker>> histogram_map_;

  DISALLOW_COPY_AND_ASSIGN(TaskSwitchMetricsRecorder);
};

}  // namespace ash

#endif  // ASH_METRICS_TASK_SWITCH_METRIC_RECORDER_H_
