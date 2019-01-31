// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater.h"

#include <memory>
#include <utility>

#include "base/at_exit.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/initialization_util.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"

namespace updater {

namespace {

void TaskSchedulerStart() {
  base::TaskScheduler::Create("Updater");
  const auto task_scheduler_init_params =
      std::make_unique<base::TaskScheduler::InitParams>(
          base::SchedulerWorkerPoolParams(
              base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.1, 0),
              base::TimeDelta::FromSeconds(30)),
          base::SchedulerWorkerPoolParams(
              base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.1, 0),
              base::TimeDelta::FromSeconds(40)),
          base::SchedulerWorkerPoolParams(
              base::RecommendedMaxNumberOfThreadsInPool(8, 32, 0.3, 0),
              base::TimeDelta::FromSeconds(30)),
          base::SchedulerWorkerPoolParams(
              base::RecommendedMaxNumberOfThreadsInPool(8, 32, 0.3, 0),
              base::TimeDelta::FromSeconds(60)));
  base::TaskScheduler::GetInstance()->Start(*task_scheduler_init_params);
}

void TaskSchedulerStop() {
  base::TaskScheduler::GetInstance()->Shutdown();
}

void QuitLoop(base::OnceClosure quit_closure) {
  std::move(quit_closure).Run();
}

}  // namespace

int UpdaterMain() {
  base::AtExitManager exit_manager;

  TaskSchedulerStart();

  base::MessageLoopForUI message_loop;
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());
  base::PlatformThread::SetName("UpdaterMain");

  // Post a task through the task scheduler to this thread's task runner
  // to make it quit the runloop and exit main.
  base::RunLoop runloop;
  auto quit_closure = base::BindOnce(
      [](scoped_refptr<base::TaskRunner> task_runner,
         base::OnceClosure quit_closure) {
        task_runner->PostTask(FROM_HERE, std::move(quit_closure));
      },
      base::ThreadTaskRunnerHandle::Get(), runloop.QuitClosure());
  base::PostTask(FROM_HERE, base::BindOnce(&QuitLoop, std::move(quit_closure)));

  runloop.Run();

  TaskSchedulerStop();
  return 0;
}

}  // namespace updater
