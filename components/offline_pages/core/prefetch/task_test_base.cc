// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/task_test_base.h"

#include "base/memory/ptr_util.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace offline_pages {

TaskTestBase::TaskTestBase()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_) {}

TaskTestBase::~TaskTestBase() = default;

void TaskTestBase::SetUp() {
  testing::Test::SetUp();
  store_test_util_.BuildStoreInMemory();
}

void TaskTestBase::TearDown() {
  store_test_util_.DeleteStore();
  RunUntilIdle();
  testing::Test::TearDown();
}

void TaskTestBase::RunUntilIdle() {
  task_runner_->RunUntilIdle();
}

void TaskTestBase::ExpectTaskCompletes(Task* task) {
  completion_callbacks_.push_back(
      base::MakeUnique<base::MockCallback<Task::TaskCompletionCallback>>());
  EXPECT_CALL(*completion_callbacks_.back(), Run(_));

  task->SetTaskCompletionCallbackForTesting(
      task_runner_.get(), completion_callbacks_.back()->Get());
}

int64_t TaskTestBase::InsertPrefetchItemInStateWithOperation(
    std::string operation_name,
    PrefetchItemState state) {
  PrefetchItem item;
  item.state = state;
  item.offline_id = PrefetchStoreUtils::GenerateOfflineId();
  std::string offline_id_string = std::to_string(item.offline_id);
  item.url = GURL("http://www.example.com/?id=" + offline_id_string);
  item.operation_name = operation_name;
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item));
  return item.offline_id;
}

std::set<PrefetchItem> TaskTestBase::FilterByState(
    const std::set<PrefetchItem>& items,
    PrefetchItemState state) const {
  std::set<PrefetchItem> result;
  for (const PrefetchItem& item : items) {
    if (item.state == state)
      result.insert(item);
  }
  return result;
}

}  // namespace offline_pages
