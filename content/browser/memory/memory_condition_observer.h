// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_MEMORY_CONDITION_OBSERVER_H_
#define CONTENT_BROWSER_MEMORY_MEMORY_CONDITION_OBSERVER_H_

#include "base/cancelable_callback.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/browser/memory/memory_coordinator_impl.h"
#include "content/common/content_export.h"

namespace content {

// MemoryConditionObserver is an internal implementation of MemoryCoordinator
// which uses a heuristic to determine the current memory condition. The
// heuristic is:
// * Compute number of renderers which can be created until the system will
//   be in a critical state. Call this N.
//   (See memory_monitor.h for the definition of "critical")
// * Covert N to memory condition (one of NORMAL/WARNING/CRITICAL) by using some
//   thresholds and hysteresis.
class CONTENT_EXPORT MemoryConditionObserver {
 public:
  // |coordinator| must outlive than this instance.
  MemoryConditionObserver(
      MemoryCoordinatorImpl* coordinator,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~MemoryConditionObserver();

  // Schedules a task to update memory condition. The task will be executed
  // after |delay| has passed.
  void ScheduleUpdateCondition(base::TimeDelta delay);

 private:
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, CalculateNextCondition);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, UpdateCondition);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, ForceSetMemoryCondition);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, DiscardTabUnderCritical);

  // Calculates next memory condition from the amount of free memory using
  // a heuristic.
  MemoryCondition CalculateNextCondition();

  // Periodically called to update the memory condition.
  void UpdateCondition();

  MemoryCoordinatorImpl* coordinator_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::CancelableClosure update_condition_closure_;

  // Sets up parameters for the heuristic.
  void InitializeParameters();

  // Validates parameters defined below.
  bool ValidateParameters();

  // Parameters to control the heuristic.

  // The median size of a renderer on the current platform. This is used to
  // convert the amount of free memory to an expected number of new renderers
  // that could be started before hitting critical memory pressure.
  int expected_renderer_size_;
  // When in a NORMAL condition and the potential number of new renderers drops
  // below this level, the coordinator will transition to a WARNING condition.
  int new_renderers_until_warning_;
  // When in a NORMAL/WARNING state and the potential number of new renderers
  // drops below this level, the coordinator will transition to a CRITICAL
  // condition.
  int new_renderers_until_critical_;
  // When in a WARNING/CRITICAL condition and the potential number of new
  // renderers rises above this level, the coordinator will transition to a
  // NORMAL condition.
  int new_renderers_back_to_normal_;
  // When in a CRITICAL condition and the potential number of new renderers
  // rises above this level, the coordinator will transition to a WARNING
  // condition.
  int new_renderers_back_to_warning_;
  // The interval of checking the amount of free memory.
  base::TimeDelta monitoring_interval_;

  DISALLOW_COPY_AND_ASSIGN(MemoryConditionObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_MEMORY_CONDITION_OBSERVER_H_
