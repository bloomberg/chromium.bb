// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/mock_sequenced_task_runner.h"

#include "base/logging.h"

MockSequencedTaskRunner::MockSequencedTaskRunner() {
}

MockSequencedTaskRunner::~MockSequencedTaskRunner() {
}

bool MockSequencedTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  NOTREACHED();
  return false;
}

bool MockSequencedTaskRunner::RunsTasksOnCurrentThread() const {
  return true;
}

bool MockSequencedTaskRunner::PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) {
  NOTREACHED();
  return false;
}
