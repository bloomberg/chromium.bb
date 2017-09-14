// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_
#define CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace base {
class MessageLoop;
class RunLoop;
}

namespace base {
class Location;
}

namespace content {

// Very few users should use this directly. To mock BrowserThreads, tests should
// use TestBrowserThreadBundle instead.
class CONTENT_EXPORT BrowserThreadImpl : public BrowserThread,
                                         public base::Thread {
 public:
  // Construct a BrowserThreadImpl with the supplied identifier.  It is an error
  // to construct a BrowserThreadImpl that already exists.
  explicit BrowserThreadImpl(BrowserThread::ID identifier);

  // Special constructor for the main (UI) thread and unittests. If a
  // |message_loop| is provied, we use a dummy thread here since the main
  // thread already exists.
  BrowserThreadImpl(BrowserThread::ID identifier,
                    base::MessageLoop* message_loop);
  ~BrowserThreadImpl() override;

  bool Start();
  bool StartWithOptions(const Options& options);
  bool StartAndWaitForTesting();

  // Redirects tasks posted to |identifier| to |task_runner|.
  static void RedirectThreadIDToTaskRunner(
      BrowserThread::ID identifier,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Makes this |identifier| no longer accept tasks and synchronously flushes
  // any tasks previously posted to it.
  // Can only be called after a matching RedirectThreadIDToTaskRunner call.
  static void StopRedirectionOfThreadID(BrowserThread::ID identifier);

  // Resets globals for |identifier|. Used in tests to clear global state that
  // would otherwise leak to the next test. Globals are not otherwise fully
  // cleaned up in ~BrowserThreadImpl() as there are subtle differences between
  // UNINITIALIZED and SHUTDOWN state (e.g. globals.task_runners are kept around
  // on shutdown). Must be called after ~BrowserThreadImpl() for the given
  // |identifier|.
  static void ResetGlobalsForTesting(BrowserThread::ID identifier);

 protected:
  void Init() override;
  void Run(base::RunLoop* run_loop) override;
  void CleanUp() override;

 private:
  // We implement all the functionality of the public BrowserThread
  // functions, but state is stored in the BrowserThreadImpl to keep
  // the API cleaner. Therefore make BrowserThread a friend class.
  friend class BrowserThread;

  // The following are unique function names that makes it possible to tell
  // the thread id from the callstack alone in crash dumps.
  void UIThreadRun(base::RunLoop* run_loop);
  void DBThreadRun(base::RunLoop* run_loop);
  void FileThreadRun(base::RunLoop* run_loop);
  void FileUserBlockingThreadRun(base::RunLoop* run_loop);
  void ProcessLauncherThreadRun(base::RunLoop* run_loop);
  void CacheThreadRun(base::RunLoop* run_loop);
  void IOThreadRun(base::RunLoop* run_loop);

  static bool PostTaskHelper(BrowserThread::ID identifier,
                             const base::Location& from_here,
                             base::OnceClosure task,
                             base::TimeDelta delay,
                             bool nestable);

  // Common initialization code for the constructors.
  void Initialize();

  // For testing.
  friend class ContentTestSuiteBaseListener;
  friend class TestBrowserThreadBundle;

  // The identifier of this thread.  Only one thread can exist with a given
  // identifier at a given time.
  ID identifier_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_THREAD_IMPL_H_
