// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_TASK_TEST_BASE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_TASK_TEST_BASE_H_

#include <memory>
#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/test/mock_callback.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/task.h"

namespace offline_pages {

// Base class for testing prefetch requests with simulated responses.
class TaskTestBase : public testing::Test {
 public:
  TaskTestBase();
  ~TaskTestBase() override;

  void SetUp() override;
  void TearDown() override;

  void RunUntilIdle();
  void ExpectTaskCompletes(Task* task);

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner() {
    return task_runner_;
  }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  std::vector<std::unique_ptr<base::MockCallback<Task::TaskCompletionCallback>>>
      completion_callbacks_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_TASK_TEST_BASE_H_
