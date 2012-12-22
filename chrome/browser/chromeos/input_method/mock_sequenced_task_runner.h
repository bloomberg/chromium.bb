// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_SEQUENCED_TASK_RUNNER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_SEQUENCED_TASK_RUNNER_H_

#include "base/basictypes.h"
#include "base/sequenced_task_runner.h"

class MockSequencedTaskRunner : public base::SequencedTaskRunner {
 public:
  MockSequencedTaskRunner();

  // base::SequencedTaskRunner implementation.
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) OVERRIDE;
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE;
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE;

 private:
  virtual ~MockSequencedTaskRunner();
  DISALLOW_COPY_AND_ASSIGN(MockSequencedTaskRunner);
};

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_SEQUENCED_TASK_RUNNER_H_
