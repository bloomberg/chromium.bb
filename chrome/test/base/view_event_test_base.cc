// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/view_event_test_base.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/view_event_test_platform_part.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/test/ui_controls.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// View subclass that allows you to specify the preferred size.
class TestView : public views::View {
 public:
  TestView() {}

  void SetPreferredSize(const gfx::Size& size) {
    preferred_size_ = size;
    PreferredSizeChanged();
  }

  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    if (!preferred_size_.IsEmpty())
      return preferred_size_;
    return View::GetPreferredSize();
  }

  virtual void Layout() OVERRIDE {
    View* child_view = child_at(0);
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
    content_view_(NULL) {
  // The TestingBrowserProcess must be created in the constructor because there
  // are tests that require it before SetUp() is called.
  TestingBrowserProcess::CreateInstance();
}

void ViewEventTestBase::Done() {
  base::MessageLoop::current()->Quit();

  // If we're in a nested message loop, as is the case with menus, we
  // need to quit twice. The second quit does that for us. Finish all
  // pending UI events before posting closure because events it may be
  // executed before UI events are executed.
  ui_controls::RunClosureAfterAllPendingUIEvents(
      base::MessageLoop::QuitClosure());
}

void ViewEventTestBase::SetUpTestCase() {
  ChromeUnitTestSuite::InitializeProviders();
  ChromeUnitTestSuite::InitializeResourceBundle();
}

void ViewEventTestBase::SetUp() {
  views::ViewsDelegate::views_delegate = &views_delegate_;
  ui::InitializeInputMethodForTesting();

  // The ContextFactory must exist before any Compositors are created.
  bool enable_pixel_output = false;
  ui::ContextFactory* context_factory =
      ui::InitializeContextFactoryForTests(enable_pixel_output);

  platform_part_.reset(ViewEventTestPlatformPart::Create(context_factory));
  gfx::NativeWindow context = platform_part_->GetContext();
  window_ = views::Widget::CreateWindowWithContext(this, context);
}

void ViewEventTestBase::TearDown() {
  if (window_) {
    window_->Close();
    content::RunAllPendingInMessageLoop();
    window_ = NULL;
  }

  ui::Clipboard::DestroyClipboardForCurrentThread();
  platform_part_.reset();

  ui::TerminateContextFactoryForTests();

  ui::ShutdownInputMethodForTesting();
  views::ViewsDelegate::views_delegate = NULL;
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

const views::Widget* ViewEventTestBase::GetWidget() const {
  return content_view_->GetWidget();
}

views::Widget* ViewEventTestBase::GetWidget() {
  return content_view_->GetWidget();
}

ViewEventTestBase::~ViewEventTestBase() {
  TestingBrowserProcess::DeleteInstance();
}

void ViewEventTestBase::StartMessageLoopAndRunTest() {
  ASSERT_TRUE(
      ui_test_utils::ShowAndFocusNativeWindow(window_->GetNativeWindow()));

  // Flush any pending events to make sure we start with a clean slate.
  content::RunAllPendingInMessageLoop();

  // Schedule a task that starts the test. Need to do this as we're going to
  // run the message loop.
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ViewEventTestBase::DoTestOnMessageLoop, this));

  content::RunMessageLoop();
}

gfx::Size ViewEventTestBase::GetPreferredSize() const {
  return gfx::Size();
}

void ViewEventTestBase::ScheduleMouseMoveInBackground(int x, int y) {
  if (!dnd_thread_.get()) {
    dnd_thread_.reset(new base::Thread("mouse-move-thread"));
    dnd_thread_->Start();
  }
  dnd_thread_->message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&ui_controls::SendMouseMove), x, y),
      base::TimeDelta::FromMilliseconds(kMouseMoveDelayMS));
}

void ViewEventTestBase::StopBackgroundThread() {
  dnd_thread_.reset(NULL);
}

void ViewEventTestBase::RunTestMethod(const base::Closure& task) {
  StopBackgroundThread();

  task.Run();
  if (HasFatalFailure())
    Done();
}
