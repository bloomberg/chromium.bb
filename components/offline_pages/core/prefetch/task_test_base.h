// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASK_TEST_BASE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASK_TEST_BASE_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/prefetch/test_prefetch_network_request_factory.h"
#include "components/offline_pages/core/task.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"

namespace offline_pages {
class Task;

// Base class for testing prefetch requests with simulated responses.
class TaskTestBase : public testing::Test {
 public:
  TaskTestBase();
  ~TaskTestBase() override;

  void ExpectTaskCompletes(Task* task);

  TestPrefetchNetworkRequestFactory* prefetch_request_factory() {
    return &prefetch_request_factory_;
  }

  net::TestURLFetcherFactory* url_fetcher_factory() {
    return &url_fetcher_factory_;
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner;

 private:
  base::ThreadTaskRunnerHandle task_runner_handle_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  TestPrefetchNetworkRequestFactory prefetch_request_factory_;

  std::vector<std::unique_ptr<base::MockCallback<Task::TaskCompletionCallback>>>
      completion_callbacks_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASK_TEST_BASE_H_
