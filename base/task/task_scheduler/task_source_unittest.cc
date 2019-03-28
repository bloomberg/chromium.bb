// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/task_source.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

class MockTaskSource : public TaskSource {
 public:
  MockTaskSource(TaskTraits traits)
      : TaskSource(traits, nullptr, TaskSourceExecutionMode::kParallel) {}

  MOCK_METHOD0(GetExecutionEnvironment, ExecutionEnvironment());

 protected:
  MOCK_METHOD0(TakeTask, base::Optional<Task>());
  MOCK_CONST_METHOD0(GetSortKey, SequenceSortKey());
  MOCK_CONST_METHOD0(IsEmpty, bool());
  MOCK_METHOD0(Clear, void());

  bool DidRunTask() override { return !IsEmpty(); }

 private:
  ~MockTaskSource() override = default;
};

}  // namespace

TEST(TaskSchedulerTaskSourceTest, TakeTaskDidRunTask) {
  scoped_refptr<MockTaskSource> task_source =
      MakeRefCounted<MockTaskSource>(TaskTraits(TaskPriority::BEST_EFFORT));
  TaskSource::Transaction task_source_transaction(
      task_source->BeginTransaction());

  EXPECT_TRUE(task_source_transaction.NeedsWorker());
  task_source_transaction.TakeTask();
  EXPECT_FALSE(task_source_transaction.NeedsWorker());

  EXPECT_TRUE(task_source_transaction.DidRunTask());
  EXPECT_TRUE(task_source_transaction.NeedsWorker());

  task_source_transaction.TakeTask();
  EXPECT_TRUE(task_source_transaction.DidRunTask());
}

TEST(TaskSchedulerTaskSourceTest, InvalidTakeTask) {
  scoped_refptr<MockTaskSource> task_source =
      MakeRefCounted<MockTaskSource>(TaskTraits(TaskPriority::BEST_EFFORT));
  TaskSource::Transaction task_source_transaction(
      task_source->BeginTransaction());

  task_source_transaction.TakeTask();
  // Can not be called before DidRunTask().
  EXPECT_DCHECK_DEATH(task_source_transaction.TakeTask());
}

TEST(TaskSchedulerTaskSourceTest, InvalidDidRunTask) {
  scoped_refptr<MockTaskSource> task_source =
      MakeRefCounted<MockTaskSource>(TaskTraits(TaskPriority::BEST_EFFORT));
  TaskSource::Transaction task_source_transaction(
      task_source->BeginTransaction());

  // Can not be called before TakeTask().
  EXPECT_DCHECK_DEATH(task_source_transaction.DidRunTask());
}

}  // namespace internal
}  // namespace base
