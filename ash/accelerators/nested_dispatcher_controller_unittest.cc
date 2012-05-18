// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/event_types.h"
#include "base/message_loop.h"
#include "ui/aura/client/dispatcher_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/events.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/base/x/x11_util.h"
#endif  // USE_X11

namespace ash {
namespace test {

namespace {

class MockDispatcher : public MessageLoop::Dispatcher {
 public:
  MockDispatcher() : num_key_events_dispatched_(0) {
  }

  int num_key_events_dispatched() { return num_key_events_dispatched_; }

#if defined(OS_WIN) || defined(USE_X11)
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE {
    if (ui::EventTypeFromNative(event) == ui::ET_KEY_RELEASED)
      num_key_events_dispatched_++;
    return !ui::IsNoopEvent(event);
  }
#endif  //  defined(OS_WIN) || defined(USE_X11)

 private:
  int num_key_events_dispatched_;
};

class TestTarget : public ui::AcceleratorTarget {
 public:
  TestTarget() : accelerator_pressed_count_(0) {
  }
  virtual ~TestTarget() {
  }

  int accelerator_pressed_count() const {
    return accelerator_pressed_count_;
  }

  // Overridden from ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE {
    accelerator_pressed_count_++;
    return true;
  }
  virtual bool CanHandleAccelerators() const OVERRIDE {
    return true;
  }

 private:
  int accelerator_pressed_count_;

  DISALLOW_COPY_AND_ASSIGN(TestTarget);
};

void DispatchKeyReleaseA() {
  // Sending both keydown and keyup is necessary here because the accelerator
  // manager only checks a keyup event following a keydown event. See
  // ShouldHandle() in ui/base/accelerators/accelerator_manager.cc for details.
#if defined(OS_WIN)
  MSG native_event_down = { NULL, WM_KEYDOWN, ui::VKEY_A, 0 };
  ash::Shell::GetRootWindow()->PostNativeEvent(native_event_down);
  MSG native_event_up = { NULL, WM_KEYUP, ui::VKEY_A, 0 };
  ash::Shell::GetRootWindow()->PostNativeEvent(native_event_up);
#elif defined(USE_X11)
  XEvent native_event;
  ui::InitXKeyEventForTesting(ui::ET_KEY_PRESSED,
                              ui::VKEY_A,
                              0,
                              &native_event);
  ash::Shell::GetRootWindow()->PostNativeEvent(&native_event);
  ui::InitXKeyEventForTesting(ui::ET_KEY_RELEASED,
                              ui::VKEY_A,
                              0,
                              &native_event);
  ash::Shell::GetRootWindow()->PostNativeEvent(&native_event);
#endif

  // Send noop event to signal dispatcher to exit.
  ash::Shell::GetRootWindow()->PostNativeEvent(ui::CreateNoopEvent());
}

}  // namespace

typedef AshTestBase NestedDispatcherTest;

// Aura window below lock screen in z order.
TEST_F(NestedDispatcherTest, AssociatedWindowBelowLockScreen) {
  MockDispatcher inner_dispatcher;
  aura::Window* default_container = Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_DefaultContainer);
  scoped_ptr<aura::Window> associated_window(aura::test::CreateTestWindowWithId(
      0, default_container));

  Shell::GetInstance()->delegate()->LockScreen();
  DispatchKeyReleaseA();
  aura::RootWindow* root_window = ash::Shell::GetInstance()->GetRootWindow();
  aura::client::GetDispatcherClient(root_window)->RunWithDispatcher(
      &inner_dispatcher,
      associated_window.get(),
      true /* nestable_tasks_allowed */);
  EXPECT_EQ(0, inner_dispatcher.num_key_events_dispatched());
  Shell::GetInstance()->delegate()->UnlockScreen();
}

// Aura window above lock screen in z order.
TEST_F(NestedDispatcherTest, AssociatedWindowAboveLockScreen) {
  MockDispatcher inner_dispatcher;

  aura::Window* default_container = Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_DefaultContainer);
  scoped_ptr<aura::Window>mock_lock_container(
      aura::test::CreateTestWindowWithId(0, default_container));
  aura::test::CreateTestWindowWithId(0, mock_lock_container.get());
  scoped_ptr<aura::Window>associated_window(aura::test::CreateTestWindowWithId(
      0, default_container));
  EXPECT_TRUE(aura::test::WindowIsAbove(associated_window.get(),
      mock_lock_container.get()));

  DispatchKeyReleaseA();
  aura::RootWindow* root_window = ash::Shell::GetInstance()->GetRootWindow();
  aura::client::GetDispatcherClient(root_window)->RunWithDispatcher(
      &inner_dispatcher,
      associated_window.get(),
      true /* nestable_tasks_allowed */);
  EXPECT_EQ(1, inner_dispatcher.num_key_events_dispatched());
}

// Test that the nested dispatcher handles accelerators.
TEST_F(NestedDispatcherTest, AcceleratorsHandled) {
  MockDispatcher inner_dispatcher;
  aura::RootWindow* root_window = ash::Shell::GetInstance()->GetRootWindow();

  ui::Accelerator accelerator(ui::VKEY_A, ui::EF_NONE);
  accelerator.set_type(ui::ET_KEY_RELEASED);
  TestTarget target;
  Shell::GetInstance()->accelerator_controller()->Register(accelerator,
                                                           &target);

  DispatchKeyReleaseA();
  aura::client::GetDispatcherClient(root_window)->RunWithDispatcher(
      &inner_dispatcher,
      root_window,
      true /* nestable_tasks_allowed */);
  EXPECT_EQ(0, inner_dispatcher.num_key_events_dispatched());
  EXPECT_EQ(1, target.accelerator_pressed_count());
}

} //  namespace test
} //  namespace ash
