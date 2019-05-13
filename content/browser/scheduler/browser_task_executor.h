// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_EXECUTOR_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_EXECUTOR_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_executor.h"
#include "build/build_config.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

class BrowserTaskExecutorTest;

// This class's job is to map base::TaskTraits to actual task queues for the
// browser process.
class CONTENT_EXPORT BrowserTaskExecutor : public base::TaskExecutor {
 public:
  // Creates and registers a BrowserTaskExecutor on the current thread which
  // owns a BrowserUIThreadScheduler. This facilitates posting tasks to a
  // BrowserThread via //base/task/post_task.h.
  static void Create();

  // As Create but with the user provided |browser_ui_thread_scheduler|.
  static void CreateWithBrowserUIThreadSchedulerForTesting(
      std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler);

  // This must be called after the FeatureList has been initialized in order
  // for scheduling experiments to function.
  static void PostFeatureListSetup();

  // Winds down the BrowserTaskExecutor, after this no tasks can be executed
  // and the base::TaskExecutor APIs are non-functional but won't crash if
  // called.
  static void Shutdown();

  static void EnableBestEffortQueues();

  // Unregister and delete the TaskExecutor after a test.
  static void ResetForTesting();

  // Runs all pending tasks for the given thread. Tasks posted after this method
  // is called (in particular any task posted from within any of the pending
  // tasks) will be queued but not run. Conceptually this call will disable all
  // queues, run any pending tasks, and re-enable all the queues.
  //
  // If any of the pending tasks posted a task, these could be run by calling
  // this method again or running a regular RunLoop. But if that were the case
  // you should probably rewrite you tests to wait for a specific event instead.
  //
  // NOTE: Can only be called from the UI thread.
  static void RunAllPendingTasksOnThreadForTesting(
      BrowserThread::ID identifier);

  // base::TaskExecutor implementation.
  bool PostDelayedTaskWithTraits(const base::Location& from_here,
                                 const base::TaskTraits& traits,
                                 base::OnceClosure task,
                                 base::TimeDelta delay) override;

  scoped_refptr<base::TaskRunner> CreateTaskRunnerWithTraits(
      const base::TaskTraits& traits) override;

  scoped_refptr<base::SequencedTaskRunner> CreateSequencedTaskRunnerWithTraits(
      const base::TaskTraits& traits) override;

  scoped_refptr<base::SingleThreadTaskRunner>
  CreateSingleThreadTaskRunnerWithTraits(
      const base::TaskTraits& traits,
      base::SingleThreadTaskRunnerThreadMode thread_mode) override;

#if defined(OS_WIN)
  scoped_refptr<base::SingleThreadTaskRunner> CreateCOMSTATaskRunnerWithTraits(
      const base::TaskTraits& traits,
      base::SingleThreadTaskRunnerThreadMode thread_mode) override;
#endif  // defined(OS_WIN)

 private:
  friend class BrowserTaskExecutorTest;

  // For GetProxyTaskRunnerForThread().
  FRIEND_TEST_ALL_PREFIXES(BrowserTaskExecutorTest,
                           EnsureUIThreadTraitPointsToExpectedQueue);
  FRIEND_TEST_ALL_PREFIXES(BrowserTaskExecutorTest,
                           EnsureIOThreadTraitPointsToExpectedQueue);
  FRIEND_TEST_ALL_PREFIXES(BrowserTaskExecutorTest,
                           BestEffortTasksRunAfterStartup);

  explicit BrowserTaskExecutor(
      std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler);
  ~BrowserTaskExecutor() override;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      const base::TaskTraits& traits);

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      const base::TaskTraits& traits,
      const BrowserTaskTraitsExtension& extension);

  static scoped_refptr<base::SingleThreadTaskRunner>
  GetProxyTaskRunnerForThread(BrowserThread::ID id);

  static scoped_refptr<base::SingleThreadTaskRunner>
  GetAfterStartupTaskRunnerForThread(BrowserThread::ID id);

  std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler_;
  BrowserUIThreadScheduler::Handle browser_ui_thread_handle_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTaskExecutor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_EXECUTOR_H_
