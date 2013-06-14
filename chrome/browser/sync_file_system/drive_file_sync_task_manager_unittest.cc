// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/sync_file_system/drive_file_sync_task_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {

namespace {

void IncrementAndAssign(int* counter,
                        SyncStatusCode* status_out,
                        SyncStatusCode status) {
  ++(*counter);
  *status_out = status;
}

class TaskManagerClient
    : public DriveFileSyncTaskManager::Client,
      public base::SupportsWeakPtr<TaskManagerClient> {
 public:
  TaskManagerClient()
      : maybe_schedule_next_task_count_(0),
        task_scheduled_count_(0),
        idle_task_scheduled_count_(0),
        last_operation_status_(SYNC_STATUS_OK),
        last_gdata_error_(google_apis::HTTP_SUCCESS) {
    task_manager_.reset(new DriveFileSyncTaskManager(AsWeakPtr()));
    task_manager_->Initialize(SYNC_STATUS_OK);
    maybe_schedule_next_task_count_ = 0;
  }
  virtual ~TaskManagerClient() {}

  // DriveFileSyncManager::Client overrides.
  virtual void MaybeScheduleNextTask() OVERRIDE {
    ++maybe_schedule_next_task_count_;
  }
  virtual void NotifyLastOperationStatus(
      SyncStatusCode last_operation_status,
      google_apis::GDataErrorCode last_gdata_error) OVERRIDE {
    last_operation_status_ = last_operation_status;
    last_gdata_error_ = last_gdata_error;
  }

  void ScheduleTask(SyncStatusCode status_to_return,
                    google_apis::GDataErrorCode gdata_error_to_return,
                    const SyncStatusCallback& callback) {
    task_manager_->ScheduleTask(
        base::Bind(&TaskManagerClient::DoTask, AsWeakPtr(),
                   status_to_return, gdata_error_to_return, false /* idle */),
        callback);
  }

  void ScheduleTaskIfIdle(SyncStatusCode status_to_return,
                          google_apis::GDataErrorCode gdata_error_to_return) {
    task_manager_->ScheduleTaskIfIdle(
        base::Bind(&TaskManagerClient::DoTask, AsWeakPtr(),
                   status_to_return, gdata_error_to_return, true /* idle */));
  }

  int maybe_schedule_next_task_count() const {
    return maybe_schedule_next_task_count_;
  }
  int task_scheduled_count() const { return task_scheduled_count_; }
  int idle_task_scheduled_count() const { return idle_task_scheduled_count_; }
  SyncStatusCode last_operation_status() const {
    return last_operation_status_;
  }
  google_apis::GDataErrorCode last_gdata_error() const {
    return last_gdata_error_;
  }

 private:
  void DoTask(SyncStatusCode status_to_return,
              google_apis::GDataErrorCode gdata_error_to_return,
              bool is_idle_task,
              const SyncStatusCallback& callback) {
    ++task_scheduled_count_;
    if (is_idle_task)
      ++idle_task_scheduled_count_;
    task_manager_->NotifyLastDriveError(gdata_error_to_return);
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, status_to_return));
  }

  scoped_ptr<DriveFileSyncTaskManager> task_manager_;

  int maybe_schedule_next_task_count_;
  int task_scheduled_count_;
  int idle_task_scheduled_count_;

  SyncStatusCode last_operation_status_;
  google_apis::GDataErrorCode last_gdata_error_;
};

// Arbitrary non-default status values for testing.
const SyncStatusCode kStatus1 = SYNC_FILE_ERROR_NO_MEMORY;
const SyncStatusCode kStatus2 = SYNC_DATABASE_ERROR_NOT_FOUND;

const google_apis::GDataErrorCode kGDataError1 = google_apis::HTTP_NOT_MODIFIED;
const google_apis::GDataErrorCode kGDataError2 = google_apis::GDATA_CANCELLED;

}  // namespace

TEST(DriveFileSyncTaskManagerTest, ScheduleTask) {
  base::MessageLoop message_loop;
  TaskManagerClient client;
  int callback_count = 0;
  SyncStatusCode callback_status = SYNC_STATUS_OK;

  client.ScheduleTask(kStatus1, kGDataError1,
                      base::Bind(&IncrementAndAssign,
                                 &callback_count,
                                 &callback_status));
  message_loop.RunUntilIdle();

  EXPECT_EQ(kStatus1, callback_status);
  EXPECT_EQ(kStatus1, client.last_operation_status());
  EXPECT_EQ(kGDataError1, client.last_gdata_error());

  EXPECT_EQ(1, callback_count);
  EXPECT_EQ(1, client.maybe_schedule_next_task_count());
  EXPECT_EQ(1, client.task_scheduled_count());
  EXPECT_EQ(0, client.idle_task_scheduled_count());
}

TEST(DriveFileSyncTaskManagerTest, ScheduleTwoTasks) {
  base::MessageLoop message_loop;
  TaskManagerClient client;
  int callback_count = 0;
  SyncStatusCode callback_status = SYNC_STATUS_OK;

  client.ScheduleTask(kStatus1, kGDataError1,
                      base::Bind(&IncrementAndAssign,
                                 &callback_count,
                                 &callback_status));
  client.ScheduleTask(kStatus2, kGDataError2,
                      base::Bind(&IncrementAndAssign,
                                 &callback_count,
                                 &callback_status));
  message_loop.RunUntilIdle();

  EXPECT_EQ(kStatus2, callback_status);
  EXPECT_EQ(kStatus2, client.last_operation_status());
  EXPECT_EQ(kGDataError2, client.last_gdata_error());

  EXPECT_EQ(2, callback_count);
  EXPECT_EQ(1, client.maybe_schedule_next_task_count());
  EXPECT_EQ(2, client.task_scheduled_count());
  EXPECT_EQ(0, client.idle_task_scheduled_count());
}

TEST(DriveFileSyncTaskManagerTest, ScheduleIdleTask) {
  base::MessageLoop message_loop;
  TaskManagerClient client;

  client.ScheduleTaskIfIdle(kStatus1, kGDataError1);
  message_loop.RunUntilIdle();

  EXPECT_EQ(kStatus1, client.last_operation_status());
  EXPECT_EQ(kGDataError1, client.last_gdata_error());

  EXPECT_EQ(1, client.maybe_schedule_next_task_count());
  EXPECT_EQ(1, client.task_scheduled_count());
  EXPECT_EQ(1, client.idle_task_scheduled_count());
}

TEST(DriveFileSyncTaskManagerTest, ScheduleIdleTaskWhileNotIdle) {
  base::MessageLoop message_loop;
  TaskManagerClient client;
  int callback_count = 0;
  SyncStatusCode callback_status = SYNC_STATUS_OK;

  client.ScheduleTask(kStatus1, kGDataError1,
                      base::Bind(&IncrementAndAssign,
                                 &callback_count,
                                 &callback_status));
  client.ScheduleTaskIfIdle(kStatus2, kGDataError2);
  message_loop.RunUntilIdle();

  // Idle task must not have run.
  EXPECT_EQ(kStatus1, callback_status);
  EXPECT_EQ(kStatus1, client.last_operation_status());
  EXPECT_EQ(kGDataError1, client.last_gdata_error());

  EXPECT_EQ(1, callback_count);
  EXPECT_EQ(1, client.maybe_schedule_next_task_count());
  EXPECT_EQ(1, client.task_scheduled_count());
  EXPECT_EQ(0, client.idle_task_scheduled_count());
}

}  // namespace sync_file_system
