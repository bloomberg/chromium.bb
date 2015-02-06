// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache_scheduler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class JobStats {
 public:
  JobStats(ServiceWorkerCacheScheduler* scheduler)
      : scheduler_(scheduler), callback_count_(0) {}

  virtual void Run() = 0;

  int callback_count() const { return callback_count_; }

 protected:
  ServiceWorkerCacheScheduler* scheduler_;
  int callback_count_;
};

class SyncJob : public JobStats {
 public:
  SyncJob(ServiceWorkerCacheScheduler* scheduler) : JobStats(scheduler) {}

  void Run() override {
    callback_count_++;
    scheduler_->CompleteOperationAndRunNext();
  }
};

class AsyncJob : public JobStats {
 public:
  AsyncJob(ServiceWorkerCacheScheduler* scheduler) : JobStats(scheduler) {}

  void Run() override { callback_count_++; }
  void Done() { scheduler_->CompleteOperationAndRunNext(); }
};

}  // namespace

class ServiceWorkerCacheSchedulerTest : public testing::Test {
 protected:
  ServiceWorkerCacheSchedulerTest()
      : async_job_(AsyncJob(&scheduler_)), sync_job_(SyncJob(&scheduler_)) {}

  ServiceWorkerCacheScheduler scheduler_;
  AsyncJob async_job_;
  SyncJob sync_job_;
};

TEST_F(ServiceWorkerCacheSchedulerTest, ScheduleOne) {
  scheduler_.ScheduleOperation(
      base::Bind(&JobStats::Run, base::Unretained(&sync_job_)));
  EXPECT_EQ(1, sync_job_.callback_count());
}

TEST_F(ServiceWorkerCacheSchedulerTest, ScheduleTwo) {
  scheduler_.ScheduleOperation(
      base::Bind(&JobStats::Run, base::Unretained(&sync_job_)));
  scheduler_.ScheduleOperation(
      base::Bind(&JobStats::Run, base::Unretained(&sync_job_)));
  EXPECT_EQ(2, sync_job_.callback_count());
}

TEST_F(ServiceWorkerCacheSchedulerTest, Block) {
  scheduler_.ScheduleOperation(
      base::Bind(&JobStats::Run, base::Unretained(&async_job_)));
  EXPECT_EQ(1, async_job_.callback_count());
  scheduler_.ScheduleOperation(
      base::Bind(&JobStats::Run, base::Unretained(&sync_job_)));
  EXPECT_EQ(0, sync_job_.callback_count());
  async_job_.Done();
  EXPECT_EQ(1, sync_job_.callback_count());
}

}  // namespace content
