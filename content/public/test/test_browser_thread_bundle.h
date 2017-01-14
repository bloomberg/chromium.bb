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
// By default, all of the created TestBrowserThreads and the task scheduler will
// be backed by a single shared MessageLoop. If a test truly needs separate
// threads, it can do so by passing the appropriate combination of option values
// during the TestBrowserThreadBundle construction.
//
// To synchronously run tasks posted to task scheduler or to TestBrowserThreads
// that use the shared MessageLoop, call RunLoop::Run/RunUntilIdle() on the
// thread where the TestBrowserThreadBundle lives. The destructor of
// TestBrowserThreadBundle runs remaining TestBrowserThreads tasks, remaining
// blocking pool tasks, and remaining BLOCK_SHUTDOWN task scheduler tasks.
//
// If a test needs a MessageLoopForIO on the main thread, it should use the
// IO_MAINLOOP option. This also allows task scheduler tasks to use
// FileDescriptorWatcher. Most of the time, IO_MAINLOOP avoids needing to use a
// REAL_IO_THREAD.
//
// If a test needs a TaskScheduler that runs tasks on a dedicated thread, it
// should use REAL_TASK_SCHEDULER. Usage of this option should be justified as
// it is easier to understand and debug a single-threaded unit test.
//
// For some tests it is important to emulate real browser startup. During real
// browser startup, the main MessageLoop is created before other threads.
// Passing DONT_CREATE_THREADS to constructor will delay creating other threads
// until the test explicitly calls CreateThreads().
//
// DONT_CREATE_THREADS should only be used when the options specify at least
// one real thread other than the main thread.

#ifndef CONTENT_PUBLIC_TEST_TEST_BROWSER_THREAD_BUNDLE_H_
#define CONTENT_PUBLIC_TEST_TEST_BROWSER_THREAD_BUNDLE_H_

#include <memory>

#include "base/macros.h"

namespace base {
class MessageLoop;
namespace test {
class ScopedAsyncTaskScheduler;
class ScopedTaskScheduler;
}  // namespace test
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
    REAL_TASK_SCHEDULER = 1 << 4,
    DONT_CREATE_THREADS = 1 << 5,
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
  std::unique_ptr<base::test::ScopedTaskScheduler> scoped_task_scheduler_;
  std::unique_ptr<TestBrowserThread> ui_thread_;
  std::unique_ptr<TestBrowserThread> db_thread_;
  std::unique_ptr<TestBrowserThread> file_thread_;
  std::unique_ptr<TestBrowserThread> file_user_blocking_thread_;
  std::unique_ptr<TestBrowserThread> process_launcher_thread_;
  std::unique_ptr<TestBrowserThread> cache_thread_;
  std::unique_ptr<TestBrowserThread> io_thread_;

  int options_;
  bool threads_created_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserThreadBundle);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_BROWSER_THREAD_BUNDLE_H_
