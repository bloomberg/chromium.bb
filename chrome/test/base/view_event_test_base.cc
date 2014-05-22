// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/view_event_test_base.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/test/ui_controls.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/message_center/message_center.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/wm_state.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/test/test_shell_delegate.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#else  // !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#endif

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
  wm_state_.reset(new wm::WMState);

  views::ViewsDelegate::views_delegate = &views_delegate_;
  ui::InitializeInputMethodForTesting();
  gfx::NativeView context = NULL;

  // The ContextFactory must exist before any Compositors are created.
  bool enable_pixel_output = false;
  ui::ContextFactory* context_factory =
      ui::InitializeContextFactoryForTests(enable_pixel_output);

#if defined(OS_CHROMEOS)
  // Ash Shell can't just live on its own without a browser process, we need to
  // also create the message center.
  message_center::MessageCenter::Initialize();
  chromeos::DBusThreadManager::InitializeWithStub();
  chromeos::CrasAudioHandler::InitializeForTesting();
  chromeos::NetworkHandler::Initialize();
  ash::test::TestShellDelegate* shell_delegate =
      new ash::test::TestShellDelegate();
  ash::ShellInitParams init_params;
  init_params.delegate = shell_delegate;
  init_params.context_factory = context_factory;
  ash::Shell::CreateInstance(init_params);
  shell_delegate->test_session_state_delegate()
      ->SetActiveUserSessionStarted(true);
  context = ash::Shell::GetPrimaryRootWindow();
  context->GetHost()->Show();
#elif defined(USE_ASH)
  // http://crbug.com/154081 use ash::Shell code path below on win_ash bots when
  // interactive_ui_tests is brought up on that platform.
  gfx::Screen::SetScreenInstance(
      gfx::SCREEN_TYPE_NATIVE, views::CreateDesktopScreen());
  aura::Env::CreateInstance(true);
  aura::Env::GetInstance()->set_context_factory(context_factory);
#elif defined(USE_AURA)
  // Instead of using the ash shell, use an AuraTestHelper to create and manage
  // the test screen.
  aura_test_helper_.reset(
      new aura::test::AuraTestHelper(base::MessageLoopForUI::current()));
  aura_test_helper_->SetUp(context_factory);
  new wm::DefaultActivationClient(aura_test_helper_->root_window());
  context = aura_test_helper_->root_window();
#endif

  window_ = views::Widget::CreateWindowWithContext(this, context);
}

void ViewEventTestBase::TearDown() {
  if (window_) {
    window_->Close();
    content::RunAllPendingInMessageLoop();
    window_ = NULL;
  }

  ui::Clipboard::DestroyClipboardForCurrentThread();

#if defined(USE_ASH)
#if defined(OS_CHROMEOS)
  ash::Shell::DeleteInstance();
  chromeos::NetworkHandler::Shutdown();
  chromeos::CrasAudioHandler::Shutdown();
  chromeos::DBusThreadManager::Shutdown();
  // Ash Shell can't just live on its own without a browser process, we need to
  // also shut down the message center.
  message_center::MessageCenter::Shutdown();
#endif
  aura::Env::DeleteInstance();
#elif defined(USE_AURA)
  aura_test_helper_->TearDown();
#endif  // !USE_ASH && USE_AURA

  ui::TerminateContextFactoryForTests();

  ui::ShutdownInputMethodForTesting();
  views::ViewsDelegate::views_delegate = NULL;

  wm_state_.reset();
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
