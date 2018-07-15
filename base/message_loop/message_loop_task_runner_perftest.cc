// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop_task_runner.h"

#include <string>
#include <utility>

#include "base/bind_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_task_runner.h"
#include "base/message_loop/message_pump.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace base {

class StubMessagePump : public MessagePump {
 public:
  StubMessagePump() = default;
  ~StubMessagePump() override = default;

  void Run(Delegate* delegate) override {}

  void Quit() override {}
  void ScheduleWork() override {}
  void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override {}
};

// Exercises MessageLoopTaskRunner+IncomingTaskQueue w/ a mock MessageLoop.
class BasicPostTaskPerfTest : public testing::Test {
 public:
  void Run(int batch_size, int tasks_per_reload) {
    base::TimeTicks start = base::TimeTicks::Now();
    base::TimeTicks now;
    MessageLoop loop(std::unique_ptr<MessagePump>(new StubMessagePump));
    scoped_refptr<internal::IncomingTaskQueue> queue(
        base::MakeRefCounted<internal::IncomingTaskQueue>(&loop));
    scoped_refptr<SingleThreadTaskRunner> task_runner(
        base::MakeRefCounted<internal::MessageLoopTaskRunner>(queue));
    uint32_t num_posted = 0;
    do {
      for (int i = 0; i < batch_size; ++i) {
        for (int j = 0; j < tasks_per_reload; ++j) {
          task_runner->PostTask(FROM_HERE, DoNothing());
          num_posted++;
        }
        TaskQueue loop_local_queue;
        queue->ReloadWorkQueue(&loop_local_queue);
        while (!loop_local_queue.empty()) {
          PendingTask t = std::move(loop_local_queue.front());
          loop_local_queue.pop();
          loop.RunTask(&t);
        }
      }

      now = base::TimeTicks::Now();
    } while (now - start < base::TimeDelta::FromSeconds(5));
    std::string trace = StringPrintf("%d_tasks_per_reload", tasks_per_reload);
    perf_test::PrintResult(
        "task", "", trace,
        (now - start).InMicroseconds() / static_cast<double>(num_posted),
        "us/task", true);
    queue->WillDestroyCurrentMessageLoop();
  }
};

TEST_F(BasicPostTaskPerfTest, OneTaskPerReload) {
  Run(10000, 1);
}

TEST_F(BasicPostTaskPerfTest, TenTasksPerReload) {
  Run(10000, 10);
}

TEST_F(BasicPostTaskPerfTest, OneHundredTasksPerReload) {
  Run(1000, 100);
}

// Exercises the full MessageLoop/RunLoop machinery.
class IntegratedPostTaskPerfTest : public testing::Test {
 public:
  void Run(int batch_size, int tasks_per_reload) {
    base::TimeTicks start = base::TimeTicks::Now();
    base::TimeTicks now;
    MessageLoop loop;
    uint32_t num_posted = 0;
    do {
      for (int i = 0; i < batch_size; ++i) {
        for (int j = 0; j < tasks_per_reload; ++j) {
          loop->task_runner()->PostTask(FROM_HERE, DoNothing());
          num_posted++;
        }
        RunLoop().RunUntilIdle();
      }

      now = base::TimeTicks::Now();
    } while (now - start < base::TimeDelta::FromSeconds(5));
    std::string trace = StringPrintf("%d_tasks_per_reload", tasks_per_reload);
    perf_test::PrintResult(
        "task", "", trace,
        (now - start).InMicroseconds() / static_cast<double>(num_posted),
        "us/task", true);
  }
};

TEST_F(IntegratedPostTaskPerfTest, OneTaskPerReload) {
  Run(10000, 1);
}

TEST_F(IntegratedPostTaskPerfTest, TenTasksPerReload) {
  Run(10000, 10);
}

TEST_F(IntegratedPostTaskPerfTest, OneHundredTasksPerReload) {
  Run(1000, 100);
}

}  // namespace base
