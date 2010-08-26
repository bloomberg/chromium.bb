// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/task_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Sets bools according to whether Run or the destructor were called.
class TrackCallsTask : public Task {
 public:
  TrackCallsTask(bool* ran, bool* deleted)
      : ran_(ran),
        deleted_(deleted) {
    *ran_ = false;
    *deleted_ = false;
  }

  virtual ~TrackCallsTask() {
    *deleted_ = true;
  }

  virtual void Run() {
    *ran_ = true;
  }

 private:
  bool* ran_;
  bool* deleted_;

  DISALLOW_COPY_AND_ASSIGN(TrackCallsTask);
};

// Adds a given task to the queue when run.
class TaskQueuerTask : public Task {
 public:
  TaskQueuerTask(TaskQueue* queue, Task* task_to_queue)
      : queue_(queue),
        task_to_queue_(task_to_queue) {
  }

  virtual void Run() {
    queue_->Push(task_to_queue_);
  }

 private:
  TaskQueue* queue_;
  Task* task_to_queue_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueuerTask);
};

}  // namespace

TEST(TaskQueueTest, RunNoTasks) {
  TaskQueue queue;
  EXPECT_TRUE(queue.IsEmpty());

  queue.Run();
  EXPECT_TRUE(queue.IsEmpty());
}

TEST(TaskQueueTest, RunTasks) {
  TaskQueue queue;

  bool ran_task1 = false;
  bool deleted_task1 = false;
  queue.Push(new TrackCallsTask(&ran_task1, &deleted_task1));

  bool ran_task2 = false;
  bool deleted_task2 = false;
  queue.Push(new TrackCallsTask(&ran_task2, &deleted_task2));

  queue.Run();

  EXPECT_TRUE(ran_task1);
  EXPECT_TRUE(deleted_task1);
  EXPECT_TRUE(ran_task2);
  EXPECT_TRUE(deleted_task2);
  EXPECT_TRUE(queue.IsEmpty());
}

TEST(TaskQueueTest, ClearTasks) {
  TaskQueue queue;

  bool ran_task1 = false;
  bool deleted_task1 = false;
  queue.Push(new TrackCallsTask(&ran_task1, &deleted_task1));

  bool ran_task2 = false;
  bool deleted_task2 = false;
  queue.Push(new TrackCallsTask(&ran_task2, &deleted_task2));

  queue.Clear();

  EXPECT_TRUE(queue.IsEmpty());

  queue.Run();

  EXPECT_FALSE(ran_task1);
  EXPECT_TRUE(deleted_task1);
  EXPECT_FALSE(ran_task2);
  EXPECT_TRUE(deleted_task2);
  EXPECT_TRUE(queue.IsEmpty());
}

TEST(TaskQueueTest, OneTaskQueuesMore) {
  TaskQueue main_queue;

  // Build a task which will queue two more when run.
  scoped_ptr<TaskQueue> nested_queue(new TaskQueue());
  bool ran_task1 = false;
  bool deleted_task1 = false;
  nested_queue->Push(
      new TaskQueuerTask(&main_queue,
                         new TrackCallsTask(&ran_task1, &deleted_task1)));
  bool ran_task2 = false;
  bool deleted_task2 = false;
  nested_queue->Push(
      new TaskQueuerTask(&main_queue,
                         new TrackCallsTask(&ran_task2, &deleted_task2)));

  main_queue.Push(nested_queue.release());

  // Run the task which pushes two more tasks.
  main_queue.Run();

  // None of the pushed tasks shoudl have run yet.
  EXPECT_FALSE(ran_task1);
  EXPECT_FALSE(deleted_task1);
  EXPECT_FALSE(ran_task2);
  EXPECT_FALSE(deleted_task2);
  EXPECT_FALSE(main_queue.IsEmpty());

  // Now run the nested tasks.
  main_queue.Run();

  EXPECT_TRUE(ran_task1);
  EXPECT_TRUE(deleted_task1);
  EXPECT_TRUE(ran_task2);
  EXPECT_TRUE(deleted_task2);
  EXPECT_TRUE(main_queue.IsEmpty());
}
