// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/scheduler_task_runner_delegate_impl.h"

#include "base/message_loop/message_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scheduler {

TEST(SchedulerTaskRunnerDelegateImplTest, TestTaskRunnerOverriding) {
  base::MessageLoop loop;
  scoped_refptr<base::SingleThreadTaskRunner> original_runner(
      loop.task_runner());
  scoped_refptr<base::SingleThreadTaskRunner> custom_runner(
      new base::TestSimpleTaskRunner());
  {
    scoped_refptr<SchedulerTaskRunnerDelegateImpl> delegate(
        SchedulerTaskRunnerDelegateImpl::Create(&loop));
    delegate->SetDefaultTaskRunner(custom_runner);
    DCHECK_EQ(custom_runner, loop.task_runner());
  }
  DCHECK_EQ(original_runner, loop.task_runner());
}

}  // namespace scheduler
