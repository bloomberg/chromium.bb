// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_MEMORY_STATE_UPDATER_H_
#define CONTENT_BROWSER_MEMORY_MEMORY_STATE_UPDATER_H_

#include "base/cancelable_callback.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/browser/memory/memory_coordinator_impl.h"
#include "content/common/content_export.h"

namespace content {

// MemoryStateUpdater is an internal implementation of MemoryCoordinator
// which uses a heuristic to determine a single global memory state.
// In the current implementation browser process and renderer processes share
// the global state; the memory coordinator will notify the global state to
// all background renderers if the state has changed.
//
// State calculation:
// MemoryStateUpdater uses followings to determine the global state:
// * Compute "number of renderers which can be created until the system will
//   be in a critical state". Call this N.
//   (See memory_monitor.h for the definition of "critical")
// * Covert N to a memory state using some thresholds/hysteresis for each state.
//   Once a state is changed to a limited state, larger N will be needed to go
//   back to a relaxed state. (e.g. THROTTLED -> NORMAL)
// * Once a state is changed, it remains the same for a certain period of time.
class CONTENT_EXPORT MemoryStateUpdater {
 public:
  // |coordinator| must outlive than this instance.
  MemoryStateUpdater(MemoryCoordinatorImpl* coordinator,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~MemoryStateUpdater();

  // Calculates the next global state from the amount of free memory using
  // a heuristic.
  base::MemoryState CalculateNextState();

  // Schedules a task to update the global state. The task will be executed
  // after |delay| has passed.
  void ScheduleUpdateState(base::TimeDelta delay);

 private:
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, CalculateNextState);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, UpdateState);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, SetMemoryStateForTesting);
  FRIEND_TEST_ALL_PREFIXES(MemoryCoordinatorImplTest, ForceSetGlobalState);

  // Periodically called to update the global state.
  void UpdateState();

  MemoryCoordinatorImpl* coordinator_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::CancelableClosure update_state_closure_;

  // Sets up parameters for the heuristic.
  void InitializeParameters();

  // Validates parameters defined below.
  bool ValidateParameters();

  // Parameters to control the heuristic.

  // The median size of a renderer on the current platform. This is used to
  // convert the amount of free memory to an expected number of new renderers
  // that could be started before hitting critical memory pressure.
  int expected_renderer_size_;
  // When in a NORMAL state and the potential number of new renderers drops
  // below this level, the coordinator will transition to a THROTTLED state.
  int new_renderers_until_throttled_;
  // When in a NORMAL/THROTTLED state and the potential number of new renderers
  // drops below this level, the coordinator will transition to a SUSPENDED
  // state.
  int new_renderers_until_suspended_;
  // When in a THROTTLED/SUSPENDED state and the potential number of new
  // renderers rises above this level, the coordinator will transition to a
  // NORMAL state.
  int new_renderers_back_to_normal_;
  // When in a SUSPENDED state and the potential number of new renderers rises
  // above this level, the coordinator will transition to a SUSPENDED state.
  int new_renderers_back_to_throttled_;
  // The memory coordinator stays in the same state at least this duration even
  // when there are considerable changes in the amount of free memory to prevent
  // thrashing.
  base::TimeDelta minimum_transition_period_;
  // The interval of checking the amount of free memory.
  base::TimeDelta monitoring_interval_;

  DISALLOW_COPY_AND_ASSIGN(MemoryStateUpdater);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_MEMORY_STATE_UPDATER_H_
