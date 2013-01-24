// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_TEST_UTILS_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "googleurl/src/gurl.h"

namespace base {
class Value;
}

// A collection of functions designed for use with unit and browser tests.

namespace content {

class RenderViewHost;

// Turns on nestable tasks, runs the message loop, then resets nestable tasks
// to what they were originally. Prefer this over MessageLoop::Run for in
// process browser tests that need to block until a condition is met.
void RunMessageLoop();

// Variant of RunMessageLoop that takes RunLoop.
void RunThisRunLoop(base::RunLoop* run_loop);

// Turns on nestable tasks, runs all pending tasks in the message loop,
// then resets nestable tasks to what they were originally. Prefer this
// over MessageLoop::RunAllPending for in process browser tests to run
// all pending tasks.
void RunAllPendingInMessageLoop();

// Blocks the current thread until all the pending messages in the loop of the
// thread |thread_id| have been processed.
void RunAllPendingInMessageLoop(BrowserThread::ID thread_id);

// Get task to quit the given RunLoop. It allows a few generations of pending
// tasks to run as opposed to run_loop->QuitClosure().
base::Closure GetQuitTaskForRunLoop(base::RunLoop* run_loop);

// Executes the specified javascript in the top-level frame, and runs a nested
// MessageLoop. When the result is available, it is returned.
// This should not be used; the use of the ExecuteScript functions in
// browser_test_utils is preferable.
scoped_ptr<base::Value> ExecuteScriptAndGetValue(
    RenderViewHost* render_view_host,
    const std::string& script);

// Helper class to Run and Quit the message loop. Run and Quit can only happen
// once per instance. Make a new instance for each use. Calling Quit after Run
// has returned is safe and has no effect.
class MessageLoopRunner : public base::RefCounted<MessageLoopRunner> {
 public:
  MessageLoopRunner();

  // Run the current MessageLoop.
  void Run();

  // Quit the matching call to Run (nested MessageLoops are unaffected).
  void Quit();

  // Hand this closure off to code that uses callbacks to notify completion.
  // Example:
  //   scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
  //   kick_off_some_api(runner->QuitClosure());
  //   runner->Run();
  base::Closure QuitClosure();

 private:
  friend class base::RefCounted<MessageLoopRunner>;
  ~MessageLoopRunner();

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopRunner);
};

// A WindowedNotificationObserver allows code to watch for a notification
// over a window of time. Typically testing code will need to do something
// like this:
//   PerformAction()
//   WaitForCompletionNotification()
// This leads to flakiness as there's a window between PerformAction returning
// and the observers getting registered, where a notification will be missed.
//
// Rather, one can do this:
//   WindowedNotificationObserver signal(...)
//   PerformAction()
//   signal.Wait()
class WindowedNotificationObserver : public NotificationObserver {
 public:
  // Register to listen for notifications of the given type from either a
  // specific source, or from all sources if |source| is
  // NotificationService::AllSources().
  WindowedNotificationObserver(int notification_type,
                               const NotificationSource& source);
  virtual ~WindowedNotificationObserver();

  // Wait until the specified notification occurs.  If the notification was
  // emitted between the construction of this object and this call then it
  // returns immediately.
  void Wait();

  // Returns NotificationService::AllSources() if we haven't observed a
  // notification yet.
  const NotificationSource& source() const {
    return source_;
  }

  const NotificationDetails& details() const {
    return details_;
  }

  // NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  bool seen_;
  bool running_;
  NotificationRegistrar registrar_;

  NotificationSource source_;
  NotificationDetails details_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(WindowedNotificationObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_UTILS_H_
