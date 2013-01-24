// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_utils.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Number of times to repost a Quit task so that the MessageLoop finishes up
// pending tasks and tasks posted by those pending tasks without risking the
// potential hang behavior of MessageLoop::QuitWhenIdle.
// The criteria for choosing this number: it should be high enough to make the
// quit act like QuitWhenIdle, while taking into account that any page which is
// animating may be rendering another frame for each quit deferral. For an
// animating page, the potential delay to quitting the RunLoop would be
// kNumQuitDeferrals * frame_render_time. Some perf tests run slow, such as
// 200ms/frame.
static const int kNumQuitDeferrals = 10;

static void DeferredQuitRunLoop(const base::Closure& quit_task,
                                int num_quit_deferrals) {
  if (num_quit_deferrals <= 0) {
    quit_task.Run();
  } else {
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&DeferredQuitRunLoop, quit_task, num_quit_deferrals - 1));
  }
}

void RunAllPendingMessageAndSendQuit(BrowserThread::ID thread_id,
                                     const base::Closure& quit_task) {
  RunAllPendingInMessageLoop();
  BrowserThread::PostTask(thread_id, FROM_HERE, quit_task);
}

// Class used handle result callbacks for ExecuteScriptAndGetValue.
class ScriptCallback {
 public:
  ScriptCallback() { }
  virtual ~ScriptCallback() { }
  void ResultCallback(const base::Value* result);

  scoped_ptr<base::Value> result() { return result_.Pass(); }

 private:
  scoped_ptr<base::Value> result_;

  DISALLOW_COPY_AND_ASSIGN(ScriptCallback);
};

void ScriptCallback::ResultCallback(const base::Value* result) {
  if (result)
    result_.reset(result->DeepCopy());
  MessageLoop::current()->Quit();
}

}  // namespace

void RunMessageLoop() {
  base::RunLoop run_loop;
  RunThisRunLoop(&run_loop);
}

void RunThisRunLoop(base::RunLoop* run_loop) {
  MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());

  // If we're running inside a browser test, we might need to allow the test
  // launcher to do extra work before/after running a nested message loop.
  TestLauncherDelegate* delegate = NULL;
#if !defined(OS_IOS)
  delegate = GetCurrentTestLauncherDelegate();
#endif
  if (delegate)
    delegate->PreRunMessageLoop(run_loop);
  run_loop->Run();
  if (delegate)
    delegate->PostRunMessageLoop();
}

void RunAllPendingInMessageLoop() {
  MessageLoop::current()->PostTask(FROM_HERE,
                                   MessageLoop::QuitWhenIdleClosure());
  RunMessageLoop();
}

void RunAllPendingInMessageLoop(BrowserThread::ID thread_id) {
  if (BrowserThread::CurrentlyOn(thread_id)) {
    RunAllPendingInMessageLoop();
    return;
  }
  BrowserThread::ID current_thread_id;
  if (!BrowserThread::GetCurrentThreadIdentifier(&current_thread_id)) {
    NOTREACHED();
    return;
  }

  base::RunLoop run_loop;
  BrowserThread::PostTask(thread_id, FROM_HERE,
      base::Bind(&RunAllPendingMessageAndSendQuit, current_thread_id,
                 run_loop.QuitClosure()));
  RunThisRunLoop(&run_loop);
}

base::Closure GetQuitTaskForRunLoop(base::RunLoop* run_loop) {
  return base::Bind(&DeferredQuitRunLoop, run_loop->QuitClosure(),
                    kNumQuitDeferrals);
}

scoped_ptr<base::Value> ExecuteScriptAndGetValue(
    RenderViewHost* render_view_host,
    const std::string& script) {
  ScriptCallback observer;

  render_view_host->ExecuteJavascriptInWebFrameCallbackResult(
      string16(),  // frame_xpath,
      UTF8ToUTF16(script),
      base::Bind(&ScriptCallback::ResultCallback, base::Unretained(&observer)));
  MessageLoop* loop = MessageLoop::current();
  loop->Run();
  return observer.result().Pass();
}

MessageLoopRunner::MessageLoopRunner()
    : loop_running_(false),
      quit_closure_called_(false) {
}

MessageLoopRunner::~MessageLoopRunner() {
}

void MessageLoopRunner::Run() {
  // Do not run the message loop if our quit closure has already been called.
  // This helps in scenarios where the closure has a chance to run before
  // we Run explicitly.
  if (quit_closure_called_)
    return;

  loop_running_ = true;
  RunThisRunLoop(&run_loop_);
}

base::Closure MessageLoopRunner::QuitClosure() {
  return base::Bind(&MessageLoopRunner::Quit, this);
}

void MessageLoopRunner::Quit() {
  quit_closure_called_ = true;

  // Only run the quit task if we are running the message loop.
  if (loop_running_) {
    GetQuitTaskForRunLoop(&run_loop_).Run();
    loop_running_ = false;
  }
}

WindowedNotificationObserver::WindowedNotificationObserver(
    int notification_type,
    const NotificationSource& source)
    : seen_(false),
      running_(false),
      source_(NotificationService::AllSources()) {
  registrar_.Add(this, notification_type, source);
}

WindowedNotificationObserver::~WindowedNotificationObserver() {}

void WindowedNotificationObserver::Wait() {
  if (seen_)
    return;

  running_ = true;
  message_loop_runner_ = new MessageLoopRunner;
  message_loop_runner_->Run();
  EXPECT_TRUE(seen_);
}

void WindowedNotificationObserver::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  source_ = source;
  details_ = details;
  seen_ = true;
  if (!running_)
    return;

  message_loop_runner_->Quit();
  running_ = false;
}

}  // namespace content
