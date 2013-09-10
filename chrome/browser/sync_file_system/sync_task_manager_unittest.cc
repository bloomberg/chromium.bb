// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/sync_file_system/sync_task_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {

namespace {

void DumbTask(SyncStatusCode status,
              const SyncStatusCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, status));
}

void IncrementAndAssign(int expected_before_counter,
                        int* counter,
                        SyncStatusCode* status_out,
                        SyncStatusCode status) {
  EXPECT_EQ(expected_before_counter, *counter);
  ++(*counter);
  *status_out = status;
}

template <typename T>
void IncrementAndAssignWithOwnedPointer(T* object,
                                        int* counter,
                                        SyncStatusCode* status_out,
                                        SyncStatusCode status) {
  ++(*counter);
  *status_out = status;
}

class TaskManagerClient
    : public SyncTaskManager::Client,
      public base::SupportsWeakPtr<TaskManagerClient> {
 public:
  TaskManagerClient()
      : maybe_schedule_next_task_count_(0),
        task_scheduled_count_(0),
        idle_task_scheduled_count_(0),
        last_operation_status_(SYNC_STATUS_OK) {
    task_manager_.reset(new SyncTaskManager(AsWeakPtr()));
    task_manager_->Initialize(SYNC_STATUS_OK);
    maybe_schedule_next_task_count_ = 0;
  }
  virtual ~TaskManagerClient() {}

  // DriveFileSyncManager::Client overrides.
  virtual void MaybeScheduleNextTask() OVERRIDE {
    ++maybe_schedule_next_task_count_;
  }
  virtual void NotifyLastOperationStatus(
      SyncStatusCode last_operation_status) OVERRIDE {
    last_operation_status_ = last_operation_status;
  }

  void ScheduleTask(SyncStatusCode status_to_return,
                    const SyncStatusCallback& callback) {
    task_manager_->ScheduleTask(
        base::Bind(&TaskManagerClient::DoTask, AsWeakPtr(),
                   status_to_return, false /* idle */),
        callback);
  }

  void ScheduleTaskIfIdle(SyncStatusCode status_to_return) {
    task_manager_->ScheduleTaskIfIdle(
        base::Bind(&TaskManagerClient::DoTask, AsWeakPtr(),
                   status_to_return, true /* idle */));
  }

  int maybe_schedule_next_task_count() const {
    return maybe_schedule_next_task_count_;
  }
  int task_scheduled_count() const { return task_scheduled_count_; }
  int idle_task_scheduled_count() const { return idle_task_scheduled_count_; }
  SyncStatusCode last_operation_status() const {
    return last_operation_status_;
  }

 private:
  void DoTask(SyncStatusCode status_to_return,
              bool is_idle_task,
              const SyncStatusCallback& callback) {
    ++task_scheduled_count_;
    if (is_idle_task)
      ++idle_task_scheduled_count_;
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, status_to_return));
  }

  scoped_ptr<SyncTaskManager> task_manager_;

  int maybe_schedule_next_task_count_;
  int task_scheduled_count_;
  int idle_task_scheduled_count_;

  SyncStatusCode last_operation_status_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerClient);
};

class MultihopSyncTask : public SyncTask {
 public:
  MultihopSyncTask(bool* task_started,
                   bool* task_completed)
      : task_started_(task_started),
        task_completed_(task_completed),
        weak_ptr_factory_(this) {
    DCHECK(task_started_);
    DCHECK(task_completed_);
  }

  virtual ~MultihopSyncTask() {}

  virtual void Run(const SyncStatusCallback& callback) OVERRIDE {
    DCHECK(!*task_started_);
    *task_started_ = true;
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&MultihopSyncTask::CompleteTask,
                              weak_ptr_factory_.GetWeakPtr(), callback));
  }

 private:
  void CompleteTask(const SyncStatusCallback& callback) {
    DCHECK(*task_started_);
    DCHECK(!*task_completed_);
    *task_completed_ = true;
    callback.Run(SYNC_STATUS_OK);
  }

  bool* task_started_;
  bool* task_completed_;
  base::WeakPtrFactory<MultihopSyncTask> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MultihopSyncTask);
};

// Arbitrary non-default status values for testing.
const SyncStatusCode kStatus1 = static_cast<SyncStatusCode>(-1);
const SyncStatusCode kStatus2 = static_cast<SyncStatusCode>(-2);
const SyncStatusCode kStatus3 = static_cast<SyncStatusCode>(-3);
const SyncStatusCode kStatus4 = static_cast<SyncStatusCode>(-4);
const SyncStatusCode kStatus5 = static_cast<SyncStatusCode>(-5);

}  // namespace

TEST(SyncTaskManagerTest, ScheduleTask) {
  base::MessageLoop message_loop;
  TaskManagerClient client;
  int callback_count = 0;
  SyncStatusCode callback_status = SYNC_STATUS_OK;

  client.ScheduleTask(kStatus1, base::Bind(&IncrementAndAssign, 0,
                                           &callback_count,
                                           &callback_status));
  message_loop.RunUntilIdle();

  EXPECT_EQ(kStatus1, callback_status);
  EXPECT_EQ(kStatus1, client.last_operation_status());

  EXPECT_EQ(1, callback_count);
  EXPECT_EQ(1, client.maybe_schedule_next_task_count());
  EXPECT_EQ(1, client.task_scheduled_count());
  EXPECT_EQ(0, client.idle_task_scheduled_count());
}

TEST(SyncTaskManagerTest, ScheduleTwoTasks) {
  base::MessageLoop message_loop;
  TaskManagerClient client;
  int callback_count = 0;
  SyncStatusCode callback_status = SYNC_STATUS_OK;

  client.ScheduleTask(kStatus1, base::Bind(&IncrementAndAssign, 0,
                                           &callback_count,
                                           &callback_status));
  client.ScheduleTask(kStatus2, base::Bind(&IncrementAndAssign, 1,
                                           &callback_count,
                                           &callback_status));
  message_loop.RunUntilIdle();

  EXPECT_EQ(kStatus2, callback_status);
  EXPECT_EQ(kStatus2, client.last_operation_status());

  EXPECT_EQ(2, callback_count);
  EXPECT_EQ(1, client.maybe_schedule_next_task_count());
  EXPECT_EQ(2, client.task_scheduled_count());
  EXPECT_EQ(0, client.idle_task_scheduled_count());
}

TEST(SyncTaskManagerTest, ScheduleIdleTask) {
  base::MessageLoop message_loop;
  TaskManagerClient client;

  client.ScheduleTaskIfIdle(kStatus1);
  message_loop.RunUntilIdle();

  EXPECT_EQ(kStatus1, client.last_operation_status());

  EXPECT_EQ(1, client.maybe_schedule_next_task_count());
  EXPECT_EQ(1, client.task_scheduled_count());
  EXPECT_EQ(1, client.idle_task_scheduled_count());
}

TEST(SyncTaskManagerTest, ScheduleIdleTaskWhileNotIdle) {
  base::MessageLoop message_loop;
  TaskManagerClient client;
  int callback_count = 0;
  SyncStatusCode callback_status = SYNC_STATUS_OK;

  client.ScheduleTask(kStatus1, base::Bind(&IncrementAndAssign, 0,
                                           &callback_count,
                                           &callback_status));
  client.ScheduleTaskIfIdle(kStatus2);
  message_loop.RunUntilIdle();

  // Idle task must not have run.
  EXPECT_EQ(kStatus1, callback_status);
  EXPECT_EQ(kStatus1, client.last_operation_status());

  EXPECT_EQ(1, callback_count);
  EXPECT_EQ(1, client.maybe_schedule_next_task_count());
  EXPECT_EQ(1, client.task_scheduled_count());
  EXPECT_EQ(0, client.idle_task_scheduled_count());
}

TEST(SyncTaskManagerTest, ScheduleAndCancelSyncTask) {
  base::MessageLoop message_loop;

  int callback_count = 0;
  SyncStatusCode status = SYNC_STATUS_UNKNOWN;

  bool task_started = false;
  bool task_completed = false;

  {
    SyncTaskManager task_manager((base::WeakPtr<SyncTaskManager::Client>()));
    task_manager.Initialize(SYNC_STATUS_OK);
    task_manager.ScheduleSyncTask(
        scoped_ptr<SyncTask>(new MultihopSyncTask(
            &task_started, &task_completed)),
        base::Bind(&IncrementAndAssign, 0, &callback_count, &status));
  }

  message_loop.RunUntilIdle();
  EXPECT_EQ(0, callback_count);
  EXPECT_EQ(SYNC_STATUS_UNKNOWN, status);
  EXPECT_TRUE(task_started);
  EXPECT_FALSE(task_completed);
}

TEST(SyncTaskManagerTest, ScheduleAndCancelTask) {
  base::MessageLoop message_loop;

  int callback_count = 0;
  SyncStatusCode status = SYNC_STATUS_UNKNOWN;

  bool task_started = false;
  bool task_completed = false;

  {
    SyncTaskManager task_manager((base::WeakPtr<SyncTaskManager::Client>()));
    task_manager.Initialize(SYNC_STATUS_OK);
    MultihopSyncTask* task = new MultihopSyncTask(
        &task_started, &task_completed);
    task_manager.ScheduleTask(
        base::Bind(&MultihopSyncTask::Run, base::Unretained(task)),
        base::Bind(&IncrementAndAssignWithOwnedPointer<MultihopSyncTask>,
                   base::Owned(task), &callback_count, &status));
  }

  message_loop.RunUntilIdle();
  EXPECT_EQ(0, callback_count);
  EXPECT_EQ(SYNC_STATUS_UNKNOWN, status);
  EXPECT_TRUE(task_started);
  EXPECT_FALSE(task_completed);
}

TEST(SyncTaskManagerTest, ScheduleTaskAtPriority) {
  base::MessageLoop message_loop;
  SyncTaskManager task_manager((base::WeakPtr<SyncTaskManager::Client>()));
  task_manager.Initialize(SYNC_STATUS_OK);

  int callback_count = 0;
  SyncStatusCode callback_status1 = SYNC_STATUS_OK;
  SyncStatusCode callback_status2 = SYNC_STATUS_OK;
  SyncStatusCode callback_status3 = SYNC_STATUS_OK;
  SyncStatusCode callback_status4 = SYNC_STATUS_OK;
  SyncStatusCode callback_status5 = SYNC_STATUS_OK;

  // This will run first even if its priority is low, since there're no
  // pending tasks.
  task_manager.ScheduleTaskAtPriority(
      base::Bind(&DumbTask, kStatus1),
      SyncTaskManager::PRIORITY_LOW,
      base::Bind(&IncrementAndAssign, 0, &callback_count, &callback_status1));

  // This runs last (expected counter == 4).
  task_manager.ScheduleTaskAtPriority(
      base::Bind(&DumbTask, kStatus2),
      SyncTaskManager::PRIORITY_LOW,
      base::Bind(&IncrementAndAssign, 4, &callback_count, &callback_status2));

  // This runs second (expected counter == 1).
  task_manager.ScheduleTaskAtPriority(
      base::Bind(&DumbTask, kStatus3),
      SyncTaskManager::PRIORITY_HIGH,
      base::Bind(&IncrementAndAssign, 1, &callback_count, &callback_status3));

  // This runs fourth (expected counter == 3).
  task_manager.ScheduleTaskAtPriority(
      base::Bind(&DumbTask, kStatus4),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&IncrementAndAssign, 3, &callback_count, &callback_status4));

  // This runs third (expected counter == 2).
  task_manager.ScheduleTaskAtPriority(
      base::Bind(&DumbTask, kStatus5),
      SyncTaskManager::PRIORITY_HIGH,
      base::Bind(&IncrementAndAssign, 2, &callback_count, &callback_status5));

  message_loop.RunUntilIdle();

  EXPECT_EQ(kStatus1, callback_status1);
  EXPECT_EQ(kStatus2, callback_status2);
  EXPECT_EQ(kStatus3, callback_status3);
  EXPECT_EQ(kStatus4, callback_status4);
  EXPECT_EQ(kStatus5, callback_status5);
  EXPECT_EQ(5, callback_count);
}

}  // namespace sync_file_system
