// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_THREAD_POOL_H_
#define BASE_TASK_THREAD_POOL_THREAD_POOL_H_

#include <memory>
#include <vector>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_executor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/thread_group_params.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace gin {
class V8Platform;
}

namespace content {
// Can't use the FRIEND_TEST_ALL_PREFIXES macro because the test is in a
// different namespace.
class BrowserMainLoopTest_CreateThreadsInSingleProcess_Test;
}  // namespace content

namespace base {

class WorkerThreadObserver;
class ThreadPoolTestHelpers;

// Interface for a thread pool and static methods to manage the instance used
// by the post_task.h API.
//
// The thread pool doesn't create threads until Start() is called. Tasks can
// be posted at any time but will not run until after Start() is called.
//
// The instance methods of this class are thread-safe.
//
// Note: All ThreadPool users should go through base/task/post_task.h instead
// of this interface except for the one callsite per process which manages the
// process's instance.
class BASE_EXPORT ThreadPool : public TaskExecutor {
 public:
  struct BASE_EXPORT InitParams {
    enum class CommonThreadPoolEnvironment {
      // Use the default environment (no environment).
      DEFAULT,
#if defined(OS_WIN)
      // Place the pool's workers in a COM MTA.
      COM_MTA,
#endif  // defined(OS_WIN)
    };

    InitParams(const ThreadGroupParams& background_thread_group_params_in,
               const ThreadGroupParams& foreground_thread_group_params_in,
               CommonThreadPoolEnvironment common_thread_pool_environment_in =
                   CommonThreadPoolEnvironment::DEFAULT);
    ~InitParams();

    ThreadGroupParams background_thread_group_params;
    ThreadGroupParams foreground_thread_group_params;
    CommonThreadPoolEnvironment common_thread_pool_environment;
  };

  // A ScopedExecutionFence prevents any new task from being scheduled in
  // ThreadPool within its scope. Upon its destruction, all tasks that were
  // preeempted are released. Note: the constructor of ScopedExecutionFence will
  // not wait for currently running tasks (as they were posted before entering
  // this scope and do not violate the contract; some of them could be
  // CONTINUE_ON_SHUTDOWN and waiting for them to complete is ill-advised).
  class BASE_EXPORT ScopedExecutionFence {
   public:
    ScopedExecutionFence();
    ~ScopedExecutionFence();

   private:
    DISALLOW_COPY_AND_ASSIGN(ScopedExecutionFence);
  };

  // Destroying a ThreadPool is not allowed in production; it is always
  // leaked. In tests, it should only be destroyed after JoinForTesting() has
  // returned.
  ~ThreadPool() override = default;

  // Allows the thread pool to create threads and run tasks following the
  // |init_params| specification.
  //
  // If specified, |worker_thread_observer| will be notified when a worker
  // enters and exits its main function. It must not be destroyed before
  // JoinForTesting() has returned (must never be destroyed in production).
  //
  // CHECKs on failure.
  virtual void Start(
      const InitParams& init_params,
      WorkerThreadObserver* worker_thread_observer = nullptr) = 0;

  // Synchronously shuts down the thread pool. Once this is called, only tasks
  // posted with the BLOCK_SHUTDOWN behavior will be run. When this returns:
  // - All SKIP_ON_SHUTDOWN tasks that were already running have completed their
  //   execution.
  // - All posted BLOCK_SHUTDOWN tasks have completed their execution.
  // - CONTINUE_ON_SHUTDOWN tasks might still be running.
  // Note that an implementation can keep threads and other resources alive to
  // support running CONTINUE_ON_SHUTDOWN after this returns. This can only be
  // called once.
  virtual void Shutdown() = 0;

  // Waits until there are no pending undelayed tasks. May be called in tests
  // to validate that a condition is met after all undelayed tasks have run.
  //
  // Does not wait for delayed tasks. Waits for undelayed tasks posted from
  // other threads during the call. Returns immediately when shutdown completes.
  virtual void FlushForTesting() = 0;

  // Returns and calls |flush_callback| when there are no incomplete undelayed
  // tasks. |flush_callback| may be called back on any thread and should not
  // perform a lot of work. May be used when additional work on the current
  // thread needs to be performed during a flush. Only one
  // FlushAsyncForTesting() may be pending at any given time.
  virtual void FlushAsyncForTesting(OnceClosure flush_callback) = 0;

  // Joins all threads. Tasks that are already running are allowed to complete
  // their execution. This can only be called once. Using this thread pool
  // instance to create task runners or post tasks is not permitted during or
  // after this call.
  virtual void JoinForTesting() = 0;

  // CreateAndStartWithDefaultParams(), Create(), and SetInstance() register a
  // ThreadPool to handle tasks posted through the post_task.h API for this
  // process.
  //
  // Processes that need to initialize ThreadPool with custom params or that
  // need to allow tasks to be posted before the ThreadPool creates its
  // threads should use Create() followed by Start(). Other processes can use
  // CreateAndStartWithDefaultParams().
  //
  // A registered ThreadPool is only deleted when a new ThreadPool is
  // registered. The last registered ThreadPool is leaked on shutdown. The
  // methods below must not be called when TaskRunners created by a previous
  // ThreadPool are still alive. The methods are not thread-safe; proper
  // synchronization is required to use the post_task.h API after registering a
  // new ThreadPool.

#if !defined(OS_NACL)
  // Creates and starts a thread pool using default params. |name| is used to
  // label histograms, it must not be empty. It should identify the component
  // that calls this. Start() is called by this method; it is invalid to call it
  // again afterwards. CHECKs on failure. For tests, prefer
  // base::test::ScopedTaskEnvironment (ensures isolation).
  static void CreateAndStartWithDefaultParams(StringPiece name);

  // Same as CreateAndStartWithDefaultParams() but allows callers to split the
  // Create() and StartWithDefaultParams() calls.
  void StartWithDefaultParams();
#endif  // !defined(OS_NACL)

  // Creates a ready to start thread pool. |name| is used to label
  // histograms, it must not be empty. It should identify the component that
  // creates the ThreadPool. The thread pool doesn't create threads until
  // Start() is called. Tasks can be posted at any time but will not run until
  // after Start() is called. For tests, prefer
  // base::test::ScopedTaskEnvironment (ensures isolation).
  static void Create(StringPiece name);

  // Registers |thread_pool| to handle tasks posted through the post_task.h
  // API for this process. For tests, prefer base::test::ScopedTaskEnvironment
  // (ensures isolation).
  static void SetInstance(std::unique_ptr<ThreadPool> thread_pool);

  // Retrieve the ThreadPool set via SetInstance() or
  // CreateAndSet(Simple|Default)ThreadPool(). This should be used very
  // rarely; most users of ThreadPool should use the post_task.h API. In
  // particular, refrain from doing
  //   if (!ThreadPool::GetInstance()) {
  //     ThreadPool::SetInstance(...);
  //     base::PostTask(...);
  //   }
  // instead make sure to SetInstance() early in one determinstic place in the
  // process' initialization phase.
  // In doubt, consult with //base/task/thread_pool/OWNERS.
  static ThreadPool* GetInstance();

 private:
  friend class ThreadPoolTestHelpers;
  friend class gin::V8Platform;
  friend class content::BrowserMainLoopTest_CreateThreadsInSingleProcess_Test;

  // Returns the maximum number of non-single-threaded non-blocked tasks posted
  // with |traits| that can run concurrently in this ThreadPool. |traits|
  // can't contain TaskPriority::BEST_EFFORT.
  //
  // Do not use this method. To process n items, post n tasks that each process
  // 1 item rather than GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated()
  // tasks that each process
  // n/GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated() items.
  //
  // TODO(fdoray): Remove this method. https://crbug.com/687264
  virtual int GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
      const TaskTraits& traits) const = 0;

  // Sets whether tasks of any / BEST_EFFORT priority are allowed to run.
  virtual void SetCanRun(bool can_run) = 0;
  virtual void SetCanRunBestEffort(bool can_run) = 0;
};

}  // namespace base

#endif  // BASE_TASK_THREAD_POOL_THREAD_POOL_H_
