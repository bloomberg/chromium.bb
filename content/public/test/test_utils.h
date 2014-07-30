// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_TEST_UTILS_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"

namespace base {
class Value;
}  // namespace base

// A collection of functions designed for use with unit and browser tests.

namespace content {

class RenderFrameHost;

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

// Runs until both the blocking pool and the current message loop are empty
// (have no more scheduled tasks) and no tasks are running.
void RunAllBlockingPoolTasksUntilIdle();

// Get task to quit the given RunLoop. It allows a few generations of pending
// tasks to run as opposed to run_loop->QuitClosure().
base::Closure GetQuitTaskForRunLoop(base::RunLoop* run_loop);

// Executes the specified JavaScript in the specified frame, and runs a nested
// MessageLoop. When the result is available, it is returned.
// This should not be used; the use of the ExecuteScript functions in
// browser_test_utils is preferable.
scoped_ptr<base::Value> ExecuteScriptAndGetValue(
    RenderFrameHost* render_frame_host, const std::string& script);

// Helper class to Run and Quit the message loop. Run and Quit can only happen
// once per instance. Make a new instance for each use. Calling Quit after Run
// has returned is safe and has no effect.
class MessageLoopRunner : public base::RefCounted<MessageLoopRunner> {
 public:
  MessageLoopRunner();

  // Run the current MessageLoop unless the quit closure
  // has already been called.
  void Run();

  // Quit the matching call to Run (nested MessageLoops are unaffected).
  void Quit();

  // Hand this closure off to code that uses callbacks to notify completion.
  // Example:
  //   scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
  //   kick_off_some_api(runner->QuitClosure());
  //   runner->Run();
  base::Closure QuitClosure();

  bool loop_running() const { return loop_running_; }

 private:
  friend class base::RefCounted<MessageLoopRunner>;
  ~MessageLoopRunner();

  // True when the message loop is running.
  bool loop_running_;

  // True after closure returned by |QuitClosure| has been called.
  bool quit_closure_called_;

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopRunner);
};

// A WindowedNotificationObserver allows code to wait until a condition is met.
// Simple conditions are specified by providing a |notification_type| and a
// |source|. When a notification of the expected type from the expected source
// is received, the condition is met.
// More complex conditions can be specified by providing a |notification_type|
// and a |callback|. The callback is called whenever the notification is fired.
// If the callback returns |true|, the condition is met. Otherwise, the
// condition is not yet met and the callback will be invoked again every time a
// notification of the expected type is received until the callback returns
// |true|. For convenience, two callback types are defined, one that is provided
// with the notification source and details, and one that is not.
//
// This helper class exists to avoid the following common pattern in tests:
//   PerformAction()
//   WaitForCompletionNotification()
// The pattern leads to flakiness as there is a window between PerformAction
// returning and the observers getting registered, where a notification will be
// missed.
//
// Rather, one can do this:
//   WindowedNotificationObserver signal(...)
//   PerformAction()
//   signal.Wait()
class WindowedNotificationObserver : public NotificationObserver {
 public:
  // Callback invoked on notifications. Should return |true| when the condition
  // being waited for is met. For convenience, there is a choice between two
  // callback types, one that is provided with the notification source and
  // details, and one that is not.
  typedef base::Callback<bool(const NotificationSource&,
                              const NotificationDetails&)>
      ConditionTestCallback;
  typedef base::Callback<bool(void)>
      ConditionTestCallbackWithoutSourceAndDetails;

  // Set up to wait for a simple condition. The condition is met when a
  // notification of the given |notification_type| from the given |source| is
  // received. To accept notifications from all sources, specify
  // NotificationService::AllSources() as |source|.
  WindowedNotificationObserver(int notification_type,
                               const NotificationSource& source);

  // Set up to wait for a complex condition. The condition is met when
  // |callback| returns |true|. The callback is invoked whenever a notification
  // of |notification_type| from any source is received.
  WindowedNotificationObserver(int notification_type,
                               const ConditionTestCallback& callback);
  WindowedNotificationObserver(
      int notification_type,
      const ConditionTestCallbackWithoutSourceAndDetails& callback);

  virtual ~WindowedNotificationObserver();

  // Adds an additional notification type to wait for. The condition will be met
  // if any of the registered notification types from their respective sources
  // is received.
  void AddNotificationType(int notification_type,
                           const NotificationSource& source);

  // Wait until the specified condition is met. If the condition is already met
  // (that is, the expected notification has already been received or the
  // given callback returns |true| already), Wait() returns immediately.
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

  ConditionTestCallback callback_;

  NotificationSource source_;
  NotificationDetails details_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(WindowedNotificationObserver);
};

// Unit tests can use code which runs in the utility process by having it run on
// an in-process utility thread. This eliminates having two code paths in
// production code to deal with unit tests, and also helps with the binary
// separation on Windows since chrome.dll doesn't need to call into Blink code
// for some utility code to handle the single process case.
// Include this class as a member variable in your test harness if you take
// advantage of this functionality to ensure that the in-process utility thread
// is torn down correctly. See http://crbug.com/316919 for more information.
// Note: this class should be declared after the TestBrowserThreadBundle and
// ShadowingAtExitManager (if it exists) as it will need to be run before they
// are torn down.
class InProcessUtilityThreadHelper : public BrowserChildProcessObserver {
 public:
  InProcessUtilityThreadHelper();
  virtual ~InProcessUtilityThreadHelper();

 private:
  virtual void BrowserChildProcessHostConnected(
      const ChildProcessData& data) OVERRIDE;
  virtual void BrowserChildProcessHostDisconnected(
      const ChildProcessData& data) OVERRIDE;

  int child_thread_count_;
  scoped_refptr<MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(InProcessUtilityThreadHelper);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_UTILS_H_
