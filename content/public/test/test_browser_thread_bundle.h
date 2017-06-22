// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TestBrowserThreadBundle is a convenience class for creating a set of
// TestBrowserThreads, a blocking pool, and a task scheduler in unit tests. For
// most tests, it is sufficient to just instantiate the TestBrowserThreadBundle
// as a member variable. It is a good idea to put the TestBrowserThreadBundle as
// the first member variable in test classes, so it is destroyed last, and the
// test threads always exist from the perspective of other classes.
//
// By default, all of the created TestBrowserThreads will be backed by a single
// shared MessageLoop. If a test truly needs separate threads, it can do so by
// passing the appropriate combination of option values during the
// TestBrowserThreadBundle construction. TaskScheduler and blocking pool tasks
// always run on dedicated threads.
//
// To synchronously run tasks from the shared MessageLoop:
//
// ... until there are no undelayed tasks in the shared MessageLoop:
//    base::RunLoop::RunUntilIdle();
//
// ... until there are no undelayed tasks in the shared MessageLoop, in
// TaskScheduler or in the blocking pool (excluding tasks not posted from the
// shared MessageLoop's thread, TaskScheduler or the blocking pool):
//    content::RunAllBlockingPoolTasksUntilIdle();
//
// ... until a condition is met:
//    base::RunLoop run_loop;
//    // Runs until a task running in the shared MessageLoop calls
//    // run_loop.Quit() or runs run_loop.QuitClosure() (&run_loop or
//    // run_loop.QuitClosure() must be kept somewhere accessible by that task).
//    run_loop.Run();
//
// To wait until there are no pending undelayed tasks in TaskScheduler or in the
// blocking pool, without running tasks from the shared MessageLoop:
//    base::TaskScheduler::GetInstance()->FlushForTesting();
//    // Note: content::BrowserThread::GetBlockingPool()->FlushForTesting() is
//    // equivalent but deprecated.
//
// The destructor of TestBrowserThreadBundle runs remaining TestBrowserThreads
// tasks, remaining blocking pool tasks, and remaining BLOCK_SHUTDOWN task
// scheduler tasks.
//
// If a test needs a MessageLoopForIO on the main thread, it should use the
// IO_MAINLOOP option. Most of the time, IO_MAINLOOP avoids needing to use a
// REAL_IO_THREAD.
//
// For some tests it is important to emulate real browser startup. During real
// browser startup, the main MessageLoop is created before other threads.
// Passing DONT_CREATE_THREADS to constructor will delay creating other threads
// until the test explicitly calls CreateThreads().
//
// DONT_CREATE_THREADS should only be used when the options specify at least
// one real thread other than the main thread.
//
// TestBrowserThreadBundle may be instantiated in a scope where there is already
// a base::test::ScopedTaskEnvironment. In that case, it will use the
// MessageLoop and the TaskScheduler provided by this
// base::test::ScopedTaskEnvironment instead of creating its own. The ability to
// have a base::test::ScopedTaskEnvironment and a TestBrowserThreadBundle in the
// same scope is useful when a fixture that inherits from a fixture that
// provides a base::test::ScopedTaskEnvironment needs to add support for browser
// threads.
//
// Basic usage:
//
//   class MyTestFixture : public testing::Test {
//    public:
//     (...)
//
//    protected:
//     // Must be the first member (or at least before any member that cares
//     // about tasks) to be initialized first and destroyed last. protected
//     // instead of private visibility will allow controlling the task
//     // environment (e.g. clock) once such features are added (see
//     // base::test::ScopedTaskEnvironment for details), until then it at least
//     // doesn't hurt :).
//     content::TestBrowserThreadBundle test_browser_thread_bundle_;
//
//     // Other members go here (or further below in private section.)
//   };

#ifndef CONTENT_PUBLIC_TEST_TEST_BROWSER_THREAD_BUNDLE_H_
#define CONTENT_PUBLIC_TEST_TEST_BROWSER_THREAD_BUNDLE_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"

namespace base {
class MessageLoop;
namespace test {
class ScopedAsyncTaskScheduler;
}  // namespace test
#if defined(OS_WIN)
namespace win {
class ScopedCOMInitializer;
}  // namespace win
#endif
}  // namespace base

namespace content {

class TestBrowserThread;

class TestBrowserThreadBundle {
 public:
  // Used to specify the type of MessageLoop that backs the UI thread, and
  // which of the named BrowserThreads should be backed by a real
  // threads. The UI thread is always the main thread in a unit test.
  enum Options {
    DEFAULT = 0,
    IO_MAINLOOP = 1 << 0,
    REAL_DB_THREAD = 1 << 1,
    REAL_FILE_THREAD = 1 << 2,
    REAL_IO_THREAD = 1 << 3,
    DONT_CREATE_THREADS = 1 << 4,
  };

  TestBrowserThreadBundle();
  explicit TestBrowserThreadBundle(int options);

  // Creates threads; should only be called from other classes if the
  // DONT_CREATE_THREADS option was used when the bundle was created.
  void CreateThreads();

  ~TestBrowserThreadBundle();

 private:
  void Init();

  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<base::test::ScopedAsyncTaskScheduler>
      scoped_async_task_scheduler_;
  std::unique_ptr<TestBrowserThread> ui_thread_;
  std::unique_ptr<TestBrowserThread> db_thread_;
  std::unique_ptr<TestBrowserThread> file_thread_;
  std::unique_ptr<TestBrowserThread> file_user_blocking_thread_;
  std::unique_ptr<TestBrowserThread> process_launcher_thread_;
  std::unique_ptr<TestBrowserThread> cache_thread_;
  std::unique_ptr<TestBrowserThread> io_thread_;

  int options_;
  bool threads_created_;

#if defined(OS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestBrowserThreadBundle);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_BROWSER_THREAD_BUNDLE_H_
