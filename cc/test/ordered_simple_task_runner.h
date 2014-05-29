// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_ORDERED_SIMPLE_TASK_RUNNER_H_
#define CC_TEST_ORDERED_SIMPLE_TASK_RUNNER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/test/test_simple_task_runner.h"

namespace cc {

// This runs pending tasks based on task's post_time + delay.
// We should not execute a delayed task sooner than some of the queued tasks
// which don't have a delay even though it is queued early.
class OrderedSimpleTaskRunner : public base::TestSimpleTaskRunner {
 public:
  OrderedSimpleTaskRunner();

  virtual void RunPendingTasks() OVERRIDE;

 protected:
  virtual ~OrderedSimpleTaskRunner();

 private:
  DISALLOW_COPY_AND_ASSIGN(OrderedSimpleTaskRunner);
};

}  // namespace cc

#endif  // CC_TEST_ORDERED_SIMPLE_TASK_RUNNER_H_
