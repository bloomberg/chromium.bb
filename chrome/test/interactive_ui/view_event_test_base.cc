// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interactive_ui/view_event_test_base.h"

#if defined(OS_WIN)
#include <ole2.h>
#endif

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/test/ui_test_utils.h"
#include "views/view.h"
#include "views/window/window.h"

namespace {

// Default delay for the time-out at which we stop message loop.
const int kTimeoutInMS = 20000;

// View subclass that allows you to specify the preferred size.
class TestView : public views::View {
 public:
  TestView() {}

  void SetPreferredSize(const gfx::Size& size) {
    preferred_size_ = size;
    PreferredSizeChanged();
  }

  gfx::Size GetPreferredSize() {
    if (!preferred_size_.IsEmpty())
      return preferred_size_;
    return View::GetPreferredSize();
  }

  virtual void Layout() {
    View* child_view = GetChildViewAt(0);
    child_view->SetBounds(0, 0, width(), height());
  }

 private:
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(TestView);
};

// Delay in background thread before posting mouse move.
const int kMouseMoveDelayMS = 200;

}  // namespace

ViewEventTestBase::ViewEventTestBase()
  : window_(NULL),
    content_view_(NULL),
    ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

void ViewEventTestBase::Done() {
  // Cancel the pending time-out.
  method_factory_.RevokeAll();

  MessageLoop::current()->Quit();

#if defined(OS_WIN)
  // We need to post a message to tickle the Dispatcher getting called and
  // exiting out of the nested loop. Without this the quit never runs.
  PostMessage(window_->GetNativeWindow(), WM_USER, 0, 0);
#endif

  // If we're in a nested message loop, as is the case with menus, we need
  // to quit twice. The second quit does that for us.
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

void ViewEventTestBase::SetUp() {
#if defined(OS_WIN)
  OleInitialize(NULL);
#endif
  window_ = views::Window::CreateChromeWindow(NULL, gfx::Rect(), this);
}

void ViewEventTestBase::TearDown() {
  if (window_) {
#if defined(OS_WIN)
    DestroyWindow(window_->GetNativeWindow());
#else
    window_->CloseWindow();
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    ui_test_utils::RunMessageLoop();
#endif
    window_ = NULL;
  }
#if defined(OS_WIN)
  OleUninitialize();
#endif
}

bool ViewEventTestBase::CanResize() const {
  return true;
}

views::View* ViewEventTestBase::GetContentsView() {
  if (!content_view_) {
    // Wrap the real view (as returned by CreateContentsView) in a View so
    // that we can customize the preferred size.
    TestView* test_view = new TestView();
    test_view->SetPreferredSize(GetPreferredSize());
    test_view->AddChildView(CreateContentsView());
    content_view_ = test_view;
  }
  return content_view_;
}

ViewEventTestBase::~ViewEventTestBase() {
}

void ViewEventTestBase::StartMessageLoopAndRunTest() {
  window_->Show();
  // Make sure the window is the foreground window, otherwise none of the
  // mouse events are going to be targeted correctly.
#if defined(OS_WIN)
  SetForegroundWindow(window_->GetNativeWindow());
#endif

  // Flush any pending events to make sure we start with a clean slate.
  MessageLoop::current()->RunAllPending();

  // Schedule a task that starts the test. Need to do this as we're going to
  // run the message loop.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &ViewEventTestBase::DoTestOnMessageLoop));

  // Start the timeout timer to prevent hangs.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&ViewEventTestBase::TimedOut),
      kTimeoutInMS);

  MessageLoop::current()->Run();
}

gfx::Size ViewEventTestBase::GetPreferredSize() {
  return gfx::Size();
}

void ViewEventTestBase::ScheduleMouseMoveInBackground(int x, int y) {
  if (!dnd_thread_.get()) {
    dnd_thread_.reset(new base::Thread("mouse-move-thread"));
    dnd_thread_->Start();
  }
  dnd_thread_->message_loop()->PostDelayedTask(
      FROM_HERE, NewRunnableFunction(&ui_controls::SendMouseMove, x, y),
      kMouseMoveDelayMS);
}

void ViewEventTestBase::StopBackgroundThread() {
  dnd_thread_.reset(NULL);
}

void ViewEventTestBase::RunTestMethod(Task* task) {
  StopBackgroundThread();

  scoped_ptr<Task> task_deleter(task);
  task->Run();
  if (HasFatalFailure())
    Done();
}

void ViewEventTestBase::TimedOut() {
  std::string error_message = "Test timed out. Each test runs for a max of ";
  error_message += base::IntToString(kTimeoutInMS);
  error_message += " ms (kTimeoutInMS).";

  GTEST_NONFATAL_FAILURE_(error_message.c_str());

  Done();
}
