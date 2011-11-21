// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_VIEW_EVENT_TEST_BASE_H_
#define CHROME_TEST_BASE_VIEW_EVENT_TEST_BASE_H_
#pragma once

// We only want to use ViewEventTestBase in test targets which properly
// isolate each test case by running each test in a separate process.
// This way if a test hangs the test launcher can reliably terminate it.
#if defined(HAS_OUT_OF_PROC_TEST_RUNNER)

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/widget/widget_delegate.h"

namespace gfx {
class Size;
}

// Base class for Views based tests that dispatch events.
//
// As views based event test involves waiting for events to be processed,
// writing a views based test is slightly different than that of writing
// other unit tests. In particular when the test fails or is done you need
// to stop the message loop. This can be done by way of invoking the Done
// method.
//
// Any delayed callbacks should be done by way of CreateEventTask.
// CreateEventTask checks to see if ASSERT_XXX has been invoked after invoking
// the task. If there was a failure Done is invoked and the test stops.
//
// ViewEventTestBase creates a Window with the View returned from
// CreateContentsView. The preferred size for the view can be customized by
// overriding GetPreferredSize. If you do not override GetPreferredSize the
// preferred size of the view returned from CreateContentsView is used.
//
// Subclasses of ViewEventTestBase must implement two methods:
// . DoTestOnMessageLoop: invoked when the message loop is running. Run your
//   test here, invoke Done when done.
// . CreateContentsView: returns the view to place in the window.
//
// Once you have created a ViewEventTestBase use the macro VIEW_TEST to define
// the fixture.
//
// I encountered weird timing problems in initiating dragging and drop that
// necessitated ugly hacks. In particular when the hook installed by
// ui_controls received the mouse event and posted a task that task was not
// processed. To work around this use the following pattern when initiating
// dnd:
//   // Schedule the mouse move at a location slightly different from where
//   // you really want to move to.
//   ui_controls::SendMouseMoveNotifyWhenDone(loc.x + 10, loc.y,
//       NewRunnableMethod(this, YYY));
//   // Then use this to schedule another mouse move.
//   ScheduleMouseMoveInBackground(loc.x, loc.y);

class ViewEventTestBase : public views::WidgetDelegate,
                          public testing::Test {
 public:
  ViewEventTestBase();

  // Invoke when done either because of failure or success. Quits the message
  // loop.
  void Done();

  // Creates a window.
  virtual void SetUp();

  // Destroys the window.
  virtual void TearDown();

  // Overridden from views::WidgetDelegate:
  virtual bool CanResize() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;

  // Overridden to do nothing so that this class can be used in runnable tasks.
  void AddRef() {}
  void Release() {}
  static bool ImplementsThreadSafeReferenceCounting() { return false; }

 protected:
  virtual ~ViewEventTestBase();

  // Returns the view that is added to the window.
  virtual views::View* CreateContentsView() = 0;

  // Called once the message loop is running.
  virtual void DoTestOnMessageLoop() = 0;

  // Invoke from test main. Shows the window, starts the message loop and
  // schedules a task that invokes DoTestOnMessageLoop.
  void StartMessageLoopAndRunTest();

  // Returns an empty Size. Subclasses that want a preferred size other than
  // that of the View returned by CreateContentsView should override this
  // appropriately.
  virtual gfx::Size GetPreferredSize();

  // Creates a task that calls the specified method back. The specified
  // method is called in such a way that if there are any test failures
  // Done is invoked.
  template <class T, class Method>
  base::Closure CreateEventTask(T* target, Method method) {
    return base::Bind(&ViewEventTestBase::RunTestMethod, this,
                      base::Bind(method, target));
  }

  // Spawns a new thread posts a MouseMove in the background.
  void ScheduleMouseMoveInBackground(int x, int y);

  views::Widget* window_;

 private:
  // Stops the thread started by ScheduleMouseMoveInBackground.
  void StopBackgroundThread();

  // Callback from CreateEventTask. Stops the background thread, runs the
  // supplied task and if there are failures invokes Done.
  void RunTestMethod(const base::Closure& task);

  // The content of the Window.
  views::View* content_view_;

  // Thread for posting background MouseMoves.
  scoped_ptr<base::Thread> dnd_thread_;

  MessageLoopForUI message_loop_;

  // Method factory used for time-outs.
  ScopedRunnableMethodFactory<ViewEventTestBase> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(ViewEventTestBase);
};

// Convenience macro for defining a ViewEventTestBase. See class description
// of ViewEventTestBase for details.
#define VIEW_TEST(test_class, name) \
  TEST_F(test_class, name) {\
    StartMessageLoopAndRunTest();\
  }

#endif  // defined(HAS_OUT_OF_PROC_TEST_RUNNER)

#endif  // CHROME_TEST_BASE_VIEW_EVENT_TEST_BASE_H_
