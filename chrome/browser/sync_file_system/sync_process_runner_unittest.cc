// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_process_runner.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {

namespace {

class FakeClient : public SyncProcessRunner::Client {
 public:
  FakeClient() : service_state_(SYNC_SERVICE_RUNNING) {}
  virtual ~FakeClient() {}

  virtual SyncServiceState GetSyncServiceState() OVERRIDE {
    return service_state_;
  }

  virtual SyncFileSystemService* GetSyncService() OVERRIDE {
    return NULL;
  }

  void set_service_state(SyncServiceState service_state) {
    service_state_ = service_state;
  }

 private:
  SyncServiceState service_state_;

  DISALLOW_COPY_AND_ASSIGN(FakeClient);
};

class FakeTimerHelper : public SyncProcessRunner::TimerHelper {
 public:
  FakeTimerHelper() {}
  virtual ~FakeTimerHelper() {}

  virtual bool IsRunning() OVERRIDE {
    return !timer_task_.is_null();
  }

  virtual void Start(const tracked_objects::Location& from_here,
                     const base::TimeDelta& delay,
                     const base::Closure& closure) OVERRIDE {
    scheduled_time_ = current_time_ + delay;
    timer_task_ = closure;
  }

  virtual base::TimeTicks Now() const OVERRIDE {
    return current_time_;
  }

  void SetCurrentTime(const base::TimeTicks& current_time) {
    current_time_ = current_time;
    if (current_time_ < scheduled_time_ || timer_task_.is_null())
      return;

    base::Closure task = timer_task_;
    timer_task_.Reset();
    task.Run();
  }

  void AdvanceToScheduledTime() {
    SetCurrentTime(scheduled_time_);
  }

  int64 GetCurrentDelay() {
    EXPECT_FALSE(timer_task_.is_null());
    return (scheduled_time_ - current_time_).InMilliseconds();
  }

 private:
  base::TimeTicks current_time_;
  base::TimeTicks scheduled_time_;
  base::Closure timer_task_;

  DISALLOW_COPY_AND_ASSIGN(FakeTimerHelper);
};

class FakeSyncProcessRunner : public SyncProcessRunner {
 public:
  FakeSyncProcessRunner(SyncProcessRunner::Client* client,
                        scoped_ptr<TimerHelper> timer_helper,
                        int max_parallel_task)
      : SyncProcessRunner("FakeSyncProcess",
                          client, timer_helper.Pass(),
                          max_parallel_task) {
  }

  virtual void StartSync(const SyncStatusCallback& callback) OVERRIDE {
    EXPECT_TRUE(running_task_.is_null());
    running_task_ = callback;
  }

  virtual ~FakeSyncProcessRunner() {
  }

  void UpdateChanges(int num_changes) {
    OnChangesUpdated(num_changes);
  }

  void CompleteTask(SyncStatusCode status) {
    ASSERT_FALSE(running_task_.is_null());
    SyncStatusCallback task = running_task_;
    running_task_.Reset();
    task.Run(status);
  }

  bool HasRunningTask() const {
    return !running_task_.is_null();
  }

 private:
  SyncStatusCallback running_task_;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncProcessRunner);
};

}  // namespace

TEST(SyncProcessRunnerTest, SingleTaskBasicTest) {
  FakeClient fake_client;
  FakeTimerHelper* fake_timer = new FakeTimerHelper();
  FakeSyncProcessRunner fake_runner(
      &fake_client,
      scoped_ptr<SyncProcessRunner::TimerHelper>(fake_timer),
      1 /* max_parallel_task */);

  base::TimeTicks base_time = base::TimeTicks::Now();
  fake_timer->SetCurrentTime(base_time);

  // SyncProcessRunner is expected not to run a task  initially.
  EXPECT_FALSE(fake_timer->IsRunning());

  // As soon as SyncProcessRunner gets a new update, it should start running
  // the timer to run a synchronization task.
  fake_runner.UpdateChanges(100);
  EXPECT_TRUE(fake_timer->IsRunning());
  EXPECT_EQ(SyncProcessRunner::kSyncDelayFastInMilliseconds,
            fake_timer->GetCurrentDelay());

  // When the time has come, the timer should fire the scheduled task.
  fake_timer->AdvanceToScheduledTime();
  EXPECT_FALSE(fake_timer->IsRunning());
  EXPECT_TRUE(fake_runner.HasRunningTask());

  // Successful completion of the task fires next synchronization task.
  fake_runner.CompleteTask(SYNC_STATUS_OK);
  EXPECT_TRUE(fake_timer->IsRunning());
  EXPECT_FALSE(fake_runner.HasRunningTask());
  EXPECT_EQ(SyncProcessRunner::kSyncDelayFastInMilliseconds,
            fake_timer->GetCurrentDelay());

  // Turn |service_state| to TEMPORARY_UNAVAILABLE and let the task fail.
  // |fake_runner| should schedule following tasks with longer delay.
  fake_timer->AdvanceToScheduledTime();
  fake_client.set_service_state(SYNC_SERVICE_TEMPORARY_UNAVAILABLE);
  fake_runner.CompleteTask(SYNC_STATUS_FAILED);
  EXPECT_EQ(SyncProcessRunner::kSyncDelaySlowInMilliseconds,
            fake_timer->GetCurrentDelay());

  // Repeated failure makes the task delay back off.
  fake_timer->AdvanceToScheduledTime();
  fake_runner.CompleteTask(SYNC_STATUS_FAILED);
  EXPECT_EQ(2 * SyncProcessRunner::kSyncDelaySlowInMilliseconds,
            fake_timer->GetCurrentDelay());

  // After |service_state| gets back to normal state, SyncProcessRunner should
  // restart rapid task invocation.
  fake_client.set_service_state(SYNC_SERVICE_RUNNING);
  fake_timer->AdvanceToScheduledTime();
  fake_runner.CompleteTask(SYNC_STATUS_OK);
  EXPECT_EQ(SyncProcessRunner::kSyncDelayFastInMilliseconds,
            fake_timer->GetCurrentDelay());

  // There's no item to sync anymore, SyncProcessRunner should schedules the
  // next with the longest delay.
  fake_runner.UpdateChanges(0);
  fake_timer->AdvanceToScheduledTime();
  fake_runner.CompleteTask(SYNC_STATUS_OK);
  EXPECT_EQ(SyncProcessRunner::kSyncDelayMaxInMilliseconds,
            fake_timer->GetCurrentDelay());
}

}  // namespace sync_file_system
