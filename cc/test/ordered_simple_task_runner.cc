// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/ordered_simple_task_runner.h"

#include <algorithm>
#include <deque>

#include "base/logging.h"

namespace {

bool TestPendingTaskComparator(const base::TestPendingTask& lhs,
                               const base::TestPendingTask& rhs) {
  return lhs.ShouldRunBefore(rhs);
}

}

namespace cc {

OrderedSimpleTaskRunner::OrderedSimpleTaskRunner() {}

OrderedSimpleTaskRunner::~OrderedSimpleTaskRunner() {}

void OrderedSimpleTaskRunner::RunPendingTasks() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Swap with a local variable to avoid re-entrancy problems.
  std::deque<base::TestPendingTask> tasks_to_run;
  tasks_to_run.swap(pending_tasks_);
  std::stable_sort(tasks_to_run.begin(),
                   tasks_to_run.end(),
                   TestPendingTaskComparator);
  for (std::deque<base::TestPendingTask>::iterator it = tasks_to_run.begin();
       it != tasks_to_run.end(); ++it) {
    it->task.Run();
  }
}

}  // namespace cc
