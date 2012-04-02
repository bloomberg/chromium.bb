// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/test/sequenced_task_runner_test_template.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/test/task_runner_test_template.h"
#include "base/threading/sequenced_worker_pool_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class SequencedWorkerPoolTaskRunnerTestDelegate {
 public:
  SequencedWorkerPoolTaskRunnerTestDelegate() {}

  ~SequencedWorkerPoolTaskRunnerTestDelegate() {
  }

  void StartTaskRunner() {
    pool_owner_.reset(
        new SequencedWorkerPoolOwner(10, "SequencedWorkerPoolTaskRunnerTest"));
    task_runner_ = pool_owner_->pool()->GetSequencedTaskRunner(
        pool_owner_->pool()->GetSequenceToken());
  }

  scoped_refptr<SequencedTaskRunner> GetTaskRunner() {
    return task_runner_;
  }

  void StopTaskRunner() {
    pool_owner_->pool()->FlushForTesting();
    pool_owner_->pool()->Shutdown();
    // Don't reset |pool_owner_| here, as the test may still hold a
    // reference to the pool.
  }

  bool TaskRunnerHandlesNonZeroDelays() const {
    // TODO(akalin): Set this to true once SequencedWorkerPool handles
    // non-zero delays.
    return false;
  }

 private:
  MessageLoop message_loop_;
  scoped_ptr<SequencedWorkerPoolOwner> pool_owner_;
  scoped_refptr<SequencedTaskRunner> task_runner_;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    SequencedWorkerPoolTaskRunner, TaskRunnerTest,
    SequencedWorkerPoolTaskRunnerTestDelegate);

INSTANTIATE_TYPED_TEST_CASE_P(
    SequencedWorkerPoolTaskRunner, SequencedTaskRunnerTest,
    SequencedWorkerPoolTaskRunnerTestDelegate);

}  // namespace base
