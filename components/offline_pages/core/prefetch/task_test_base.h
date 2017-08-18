// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASK_TEST_BASE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASK_TEST_BASE_H_

#include <memory>
#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/prefetch/mock_prefetch_item_generator.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/test_prefetch_network_request_factory.h"
#include "components/offline_pages/core/task.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"

namespace offline_pages {
struct PrefetchItem;
class PrefetchStore;
class Task;

// Base class for testing prefetch requests with simulated responses.
class TaskTestBase : public testing::Test {
 public:
  TaskTestBase();
  ~TaskTestBase() override;

  void SetUp() override;
  void TearDown() override;

  void RunUntilIdle();
  void ExpectTaskCompletes(Task* task);

  TestPrefetchNetworkRequestFactory* prefetch_request_factory() {
    return &prefetch_request_factory_;
  }

  net::TestURLFetcherFactory* url_fetcher_factory() {
    return &url_fetcher_factory_;
  }

  PrefetchStore* store() { return store_test_util_.store(); }

  PrefetchStoreTestUtil* store_util() { return &store_test_util_; }

  MockPrefetchItemGenerator* item_generator() { return &item_generator_; }

  int64_t InsertPrefetchItemInStateWithOperation(std::string operation_name,
                                                 PrefetchItemState state);

  std::set<PrefetchItem> FilterByState(const std::set<PrefetchItem>& items,
                                       PrefetchItemState state) const;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  TestPrefetchNetworkRequestFactory prefetch_request_factory_;
  PrefetchStoreTestUtil store_test_util_;
  MockPrefetchItemGenerator item_generator_;

  std::vector<std::unique_ptr<base::MockCallback<Task::TaskCompletionCallback>>>
      completion_callbacks_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASK_TEST_BASE_H_
