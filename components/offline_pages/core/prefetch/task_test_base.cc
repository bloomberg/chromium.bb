// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/task_test_base.h"

#include "base/memory/ptr_util.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace offline_pages {

TaskTestBase::TaskTestBase()
    : task_runner(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner) {}

TaskTestBase::~TaskTestBase() = default;

void TaskTestBase::ExpectTaskCompletes(Task* task) {
  completion_callbacks_.push_back(
      base::MakeUnique<base::MockCallback<Task::TaskCompletionCallback>>());
  EXPECT_CALL(*completion_callbacks_.back(), Run(_));

  task->SetTaskCompletionCallbackForTesting(
      task_runner.get(), completion_callbacks_.back()->Get());
}

}  // namespace offline_pages
